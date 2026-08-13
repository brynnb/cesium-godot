// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"

#include "Runtime/Private/Bounds/CesiumBoundingVolumeSnapshot.h"
#include "Utils/CesiumMathUtils.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <CesiumGeometry/AxisAlignedBox.h>
#include <CesiumGeometry/BoundingCylinderRegion.h>
#include <CesiumGeometry/BoundingSphere.h>
#include <CesiumGeometry/OrientedBoundingBox.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/BoundingRegionWithLooseFittingHeights.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/S2CellBoundingVolume.h>
#include <CesiumUtility/Math.h>

#include <cmath>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

namespace {
PackedFloat64Array vector_components(const glm::dvec3& value) {
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, value.x);
	result.set(1, value.y);
	result.set(2, value.z);
	return result;
}

PackedFloat64Array pair_components(const glm::dvec2& value) {
	PackedFloat64Array result;
	result.resize(2);
	result.set(0, value.x);
	result.set(1, value.y);
	return result;
}

PackedFloat64Array matrix_components(const glm::dmat3& value) {
	PackedFloat64Array result;
	result.resize(9);
	int32_t output = 0;
	for (int32_t column = 0; column < 3; ++column) {
		for (int32_t row = 0; row < 3; ++row) {
			result.set(output++, value[column][row]);
		}
	}
	return result;
}

CesiumGeometry::OrientedBoundingBox get_oriented_box(
	const Cesium3DTilesSelection::BoundingVolume& volume
) {
	return Cesium3DTilesSelection::getOrientedBoundingBoxFromBoundingVolume(
		volume,
		CesiumGeospatial::Ellipsoid::WGS84
	);
}

AABB axis_aligned_box_to_godot(
	const CesiumGeometry::AxisAlignedBox& box
) {
	return AABB(
		Vector3(box.minimumX, box.minimumY, box.minimumZ),
		Vector3(box.lengthX, box.lengthY, box.lengthZ)
	);
}

double distance_squared_to_position(
	const Cesium3DTilesSelection::BoundingVolume& volume,
	const glm::dvec3& position
) {
	return std::visit([
		&position
	](const auto& value) -> double {
		using Value = std::decay_t<decltype(value)>;
		if constexpr (
			std::is_same_v<Value, CesiumGeometry::BoundingSphere> ||
			std::is_same_v<Value, CesiumGeometry::OrientedBoundingBox> ||
			std::is_same_v<Value, CesiumGeospatial::S2CellBoundingVolume> ||
			std::is_same_v<Value, CesiumGeometry::BoundingCylinderRegion>
		) {
			return value.computeDistanceSquaredToPosition(position);
		} else if constexpr (
			std::is_same_v<Value, CesiumGeospatial::BoundingRegion>
		) {
			return value.computeDistanceSquaredToPosition(
				position,
				CesiumGeospatial::Ellipsoid::WGS84
			);
		} else {
			return value.getBoundingRegion().computeDistanceSquaredToPosition(
				position,
				CesiumGeospatial::Ellipsoid::WGS84
			);
		}
	}, volume);
}

bool components_to_vector(
	const PackedFloat64Array& components,
	glm::dvec3* result
) {
	if (result == nullptr || components.size() != 3) {
		return false;
	}
	*result = glm::dvec3(components[0], components[1], components[2]);
	return std::isfinite(result->x) && std::isfinite(result->y) &&
		std::isfinite(result->z);
}

double distance_from_squared(double distanceSquared) {
	return std::isfinite(distanceSquared) && distanceSquared >= 0.0
		? std::sqrt(distanceSquared)
		: std::numeric_limits<double>::infinity();
}
} // namespace

void CesiumBoundingVolume::initialize(
	const std::shared_ptr<const CesiumBoundingVolumeSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

bool CesiumBoundingVolume::is_valid() const {
	return this->m_snapshot != nullptr;
}

int32_t CesiumBoundingVolume::get_volume_type() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->volume.index()) + 1
		: static_cast<int32_t>(Type::Invalid);
}

String CesiumBoundingVolume::get_volume_type_name() const {
	switch (static_cast<Type>(this->get_volume_type())) {
	case Type::Sphere:
		return "sphere";
	case Type::OrientedBox:
		return "oriented_box";
	case Type::Region:
		return "region";
	case Type::RegionWithLooseFittingHeights:
		return "region_with_loose_fitting_heights";
	case Type::S2Cell:
		return "s2_cell";
	case Type::CylinderRegion:
		return "cylinder_region";
	case Type::Invalid:
	default:
		return "none";
	}
}

Vector3 CesiumBoundingVolume::get_center() const {
	if (!this->m_snapshot) {
		return Vector3();
	}
	return CesiumMathUtils::from_glm_vec3(
		Cesium3DTilesSelection::getBoundingVolumeCenter(
			this->m_snapshot->volume
		)
	);
}

PackedFloat64Array CesiumBoundingVolume::get_center_components() const {
	return this->m_snapshot
		? vector_components(Cesium3DTilesSelection::getBoundingVolumeCenter(
			this->m_snapshot->volume
		))
		: PackedFloat64Array();
}

Transform3D CesiumBoundingVolume::get_oriented_box_transform() const {
	if (!this->m_snapshot) {
		return Transform3D();
	}
	const CesiumGeometry::OrientedBoundingBox box = get_oriented_box(
		this->m_snapshot->volume
	);
	const glm::dmat3& halfAxes = box.getHalfAxes();
	return Transform3D(
		Basis(
			CesiumMathUtils::from_glm_vec3(halfAxes[0]),
			CesiumMathUtils::from_glm_vec3(halfAxes[1]),
			CesiumMathUtils::from_glm_vec3(halfAxes[2])
		),
		CesiumMathUtils::from_glm_vec3(box.getCenter())
	);
}

PackedFloat64Array CesiumBoundingVolume::get_oriented_box_half_axes() const {
	return this->m_snapshot
		? matrix_components(get_oriented_box(this->m_snapshot->volume).getHalfAxes())
		: PackedFloat64Array();
}

Vector3 CesiumBoundingVolume::get_oriented_box_size() const {
	return this->m_snapshot
		? CesiumMathUtils::from_glm_vec3(
			get_oriented_box(this->m_snapshot->volume).getLengths()
		)
		: Vector3();
}

AABB CesiumBoundingVolume::get_source_aabb() const {
	return this->m_snapshot
		? axis_aligned_box_to_godot(
			get_oriented_box(this->m_snapshot->volume).toAxisAligned()
		)
		: AABB();
}

AABB CesiumBoundingVolume::get_transformed_aabb(
	const Transform3D& sourceToTarget
) const {
	if (!this->m_snapshot) {
		return AABB();
	}
	return axis_aligned_box_to_godot(
		get_oriented_box(this->m_snapshot->volume)
			.transform(CesiumMathUtils::to_glm_mat4(sourceToTarget))
			.toAxisAligned()
	);
}

Dictionary CesiumBoundingVolume::get_details() const {
	Dictionary result;
	result["type"] = this->get_volume_type();
	result["type_name"] = this->get_volume_type_name();
	if (!this->m_snapshot) {
		return result;
	}
	const auto& volume = this->m_snapshot->volume;
	const glm::dvec3 center =
		Cesium3DTilesSelection::getBoundingVolumeCenter(volume);
	result["center"] = CesiumMathUtils::from_glm_vec3(center);
	result["center_components"] = vector_components(center);
	result["oriented_box_transform"] = this->get_oriented_box_transform();
	result["oriented_box_half_axes"] = this->get_oriented_box_half_axes();
	result["oriented_box_size"] = this->get_oriented_box_size();
	result["source_aabb"] = this->get_source_aabb();

	std::visit([&result](const auto& value) {
		using Value = std::decay_t<decltype(value)>;
		if constexpr (std::is_same_v<Value, CesiumGeometry::BoundingSphere>) {
			result["radius"] = value.getRadius();
		} else if constexpr (
			std::is_same_v<Value, CesiumGeospatial::BoundingRegion> ||
			std::is_same_v<
				Value,
				CesiumGeospatial::BoundingRegionWithLooseFittingHeights
			>
		) {
			const CesiumGeospatial::BoundingRegion& region = [&value]()
				-> const CesiumGeospatial::BoundingRegion& {
				if constexpr (std::is_same_v<
					Value,
					CesiumGeospatial::BoundingRegion
				>) {
					return value;
				} else {
					return value.getBoundingRegion();
				}
			}();
			const CesiumGeospatial::GlobeRectangle& rectangle =
				region.getRectangle();
			PackedFloat64Array radians;
			radians.resize(4);
			radians.set(0, rectangle.getWest());
			radians.set(1, rectangle.getSouth());
			radians.set(2, rectangle.getEast());
			radians.set(3, rectangle.getNorth());
			PackedFloat64Array degrees;
			degrees.resize(4);
			for (int32_t index = 0; index < 4; ++index) {
				degrees.set(
					index,
					CesiumUtility::Math::radiansToDegrees(radians[index])
				);
			}
			result["rectangle_radians"] = radians;
			result["rectangle_degrees"] = degrees;
			result["minimum_height"] = region.getMinimumHeight();
			result["maximum_height"] = region.getMaximumHeight();
		} else if constexpr (
			std::is_same_v<Value, CesiumGeospatial::S2CellBoundingVolume>
		) {
			result["s2_cell_token"] = String(value.getCellID().toToken().c_str());
			result["s2_cell_level"] = value.getCellID().getLevel();
			result["minimum_height"] = value.getMinimumHeight();
			result["maximum_height"] = value.getMaximumHeight();
		} else if constexpr (
			std::is_same_v<Value, CesiumGeometry::BoundingCylinderRegion>
		) {
			const glm::dquat& rotation = value.getRotation();
			PackedFloat64Array rotationComponents;
			rotationComponents.resize(4);
			rotationComponents.set(0, rotation.x);
			rotationComponents.set(1, rotation.y);
			rotationComponents.set(2, rotation.z);
			rotationComponents.set(3, rotation.w);
			result["translation"] = CesiumMathUtils::from_glm_vec3(
				value.getTranslation()
			);
			result["translation_components"] = vector_components(
				value.getTranslation()
			);
			result["rotation_components"] = rotationComponents;
			result["height"] = value.getHeight();
			result["radial_bounds"] = pair_components(value.getRadialBounds());
			result["angular_bounds"] = pair_components(value.getAngularBounds());
		}
	}, volume);
	return result;
}

Ref<CesiumBoundingVolume> CesiumBoundingVolume::create_owned_copy() const {
	Ref<CesiumBoundingVolume> result;
	result.instantiate();
	if (this->m_snapshot != nullptr) {
		result->initialize(create_cesium_bounding_volume_snapshot(
			this->m_snapshot->volume
		));
	}
	return result;
}

bool CesiumBoundingVolume::contains_source_position(
	const Vector3& position
) const {
	if (!this->m_snapshot) {
		return false;
	}
	const double distanceSquared = distance_squared_to_position(
		this->m_snapshot->volume,
		CesiumMathUtils::to_glm_dvec3(position)
	);
	return std::isfinite(distanceSquared) && distanceSquared <= 1e-12;
}

bool CesiumBoundingVolume::contains_source_position_components(
	const PackedFloat64Array& position
) const {
	glm::dvec3 sourcePosition;
	if (!this->m_snapshot || !components_to_vector(position, &sourcePosition)) {
		return false;
	}
	const double distanceSquared = distance_squared_to_position(
		this->m_snapshot->volume,
		sourcePosition
	);
	return std::isfinite(distanceSquared) && distanceSquared <= 1e-12;
}

double CesiumBoundingVolume::distance_to_source_position(
	const Vector3& position
) const {
	if (!this->m_snapshot) {
		return std::numeric_limits<double>::infinity();
	}
	return distance_from_squared(distance_squared_to_position(
		this->m_snapshot->volume,
		CesiumMathUtils::to_glm_dvec3(position)
	));
}

double CesiumBoundingVolume::distance_to_source_position_components(
	const PackedFloat64Array& position
) const {
	glm::dvec3 sourcePosition;
	if (!this->m_snapshot || !components_to_vector(position, &sourcePosition)) {
		return std::numeric_limits<double>::infinity();
	}
	return distance_from_squared(distance_squared_to_position(
		this->m_snapshot->volume,
		sourcePosition
	));
}

void CesiumBoundingVolume::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumBoundingVolume::is_valid);
	ClassDB::bind_method(D_METHOD("get_volume_type"), &CesiumBoundingVolume::get_volume_type);
	ClassDB::bind_method(D_METHOD("get_volume_type_name"), &CesiumBoundingVolume::get_volume_type_name);
	ClassDB::bind_method(D_METHOD("get_center"), &CesiumBoundingVolume::get_center);
	ClassDB::bind_method(D_METHOD("get_center_components"), &CesiumBoundingVolume::get_center_components);
	ClassDB::bind_method(D_METHOD("get_oriented_box_transform"), &CesiumBoundingVolume::get_oriented_box_transform);
	ClassDB::bind_method(D_METHOD("get_oriented_box_half_axes"), &CesiumBoundingVolume::get_oriented_box_half_axes);
	ClassDB::bind_method(D_METHOD("get_oriented_box_size"), &CesiumBoundingVolume::get_oriented_box_size);
	ClassDB::bind_method(D_METHOD("get_source_aabb"), &CesiumBoundingVolume::get_source_aabb);
	ClassDB::bind_method(D_METHOD("get_transformed_aabb", "source_to_target"), &CesiumBoundingVolume::get_transformed_aabb);
	ClassDB::bind_method(D_METHOD("get_details"), &CesiumBoundingVolume::get_details);
	ClassDB::bind_method(D_METHOD("create_owned_copy"), &CesiumBoundingVolume::create_owned_copy);
	ClassDB::bind_method(D_METHOD("contains_source_position", "position"), &CesiumBoundingVolume::contains_source_position);
	ClassDB::bind_method(D_METHOD("contains_source_position_components", "position"), &CesiumBoundingVolume::contains_source_position_components);
	ClassDB::bind_method(D_METHOD("distance_to_source_position", "position"), &CesiumBoundingVolume::distance_to_source_position);
	ClassDB::bind_method(D_METHOD("distance_to_source_position_components", "position"), &CesiumBoundingVolume::distance_to_source_position_components);

	BIND_ENUM_CONSTANT(Invalid);
	BIND_ENUM_CONSTANT(Sphere);
	BIND_ENUM_CONSTANT(OrientedBox);
	BIND_ENUM_CONSTANT(Region);
	BIND_ENUM_CONSTANT(RegionWithLooseFittingHeights);
	BIND_ENUM_CONSTANT(S2Cell);
	BIND_ENUM_CONSTANT(CylinderRegion);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "volume_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_volume_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "volume_type_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_volume_type_name");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_center");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "center_components", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_center_components");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "oriented_box_transform", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_oriented_box_transform");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "oriented_box_half_axes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_oriented_box_half_axes");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "oriented_box_size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_oriented_box_size");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "source_aabb", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_source_aabb");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "details", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_details");
}
