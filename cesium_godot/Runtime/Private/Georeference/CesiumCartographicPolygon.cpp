// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumCartographicPolygon.h"

#include "Runtime/Public/Georeference/CesiumEllipsoid.h"

#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/GlobeRectangle.h>

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/math.hpp"
#endif

#include <cmath>
#include <optional>

namespace {
bool unpack_rectangle(
	const PackedFloat64Array& values,
	CesiumGeospatial::GlobeRectangle* result
) {
	if (values.size() != 4) {
		ERR_PRINT("A cartographic rectangle must contain west, south, east, and north.");
		return false;
	}
	for (int32_t index = 0; index < 4; ++index) {
		if (!std::isfinite(values[index])) {
			ERR_PRINT("A cartographic rectangle must contain only finite values.");
			return false;
		}
	}
	*result = CesiumGeospatial::GlobeRectangle(
		Math::deg_to_rad(values[0]),
		Math::deg_to_rad(values[1]),
		Math::deg_to_rad(values[2]),
		Math::deg_to_rad(values[3])
	);
	return true;
}

bool vertices_equal(
	const std::vector<glm::dvec2>& left,
	const std::vector<glm::dvec2>& right
) {
	return left == right;
}
} // namespace

void CesiumCartographicPolygon::set_vertices_degrees(
	const PackedVector2Array& vertices
) {
	std::vector<glm::dvec2> converted;
	converted.reserve(static_cast<size_t>(vertices.size()));
	for (int32_t index = 0; index < vertices.size(); ++index) {
		const Vector2& vertex = vertices[index];
		if (!std::isfinite(static_cast<double>(vertex.x)) ||
			!std::isfinite(static_cast<double>(vertex.y))) {
			ERR_PRINT("Cartographic polygon vertices must contain only finite values.");
			return;
		}
		converted.emplace_back(
			Math::deg_to_rad(static_cast<double>(vertex.x)),
			Math::deg_to_rad(static_cast<double>(vertex.y))
		);
	}
	this->set_vertices_radians(std::move(converted));
}

PackedVector2Array CesiumCartographicPolygon::get_vertices_degrees() const {
	PackedVector2Array result;
	result.resize(static_cast<int32_t>(this->m_verticesRadians.size()));
	for (int32_t index = 0; index < result.size(); ++index) {
		const glm::dvec2& vertex = this->m_verticesRadians[static_cast<size_t>(index)];
		result.set(index, Vector2(
			static_cast<real_t>(Math::rad_to_deg(vertex.x)),
			static_cast<real_t>(Math::rad_to_deg(vertex.y))
		));
	}
	return result;
}

bool CesiumCartographicPolygon::set_vertices_degrees_exact(
	const PackedFloat64Array& components
) {
	if (components.size() % 2 != 0) {
		ERR_PRINT("Exact cartographic polygon vertices must be longitude/latitude pairs.");
		return false;
	}
	std::vector<glm::dvec2> converted;
	converted.reserve(static_cast<size_t>(components.size() / 2));
	for (int32_t index = 0; index < components.size(); index += 2) {
		if (!std::isfinite(components[index]) ||
			!std::isfinite(components[index + 1])) {
			ERR_PRINT("Cartographic polygon vertices must contain only finite values.");
			return false;
		}
		converted.emplace_back(
			Math::deg_to_rad(components[index]),
			Math::deg_to_rad(components[index + 1])
		);
	}
	this->set_vertices_radians(std::move(converted));
	return true;
}

PackedFloat64Array
CesiumCartographicPolygon::get_vertices_degrees_exact() const {
	PackedFloat64Array result;
	result.resize(static_cast<int32_t>(this->m_verticesRadians.size() * 2));
	int32_t output = 0;
	for (const glm::dvec2& vertex : this->m_verticesRadians) {
		result.set(output++, Math::rad_to_deg(vertex.x));
		result.set(output++, Math::rad_to_deg(vertex.y));
	}
	return result;
}

bool CesiumCartographicPolygon::set_vertices_from_ecef_exact(
	const PackedFloat64Array& ecefComponents,
	const Ref<CesiumEllipsoid>& ellipsoid
) {
	if (ellipsoid.is_null() || !ellipsoid->is_valid()) {
		ERR_PRINT("An exact ECEF polygon requires a valid CesiumEllipsoid.");
		return false;
	}
	if (ecefComponents.size() % 3 != 0) {
		ERR_PRINT("Exact ECEF polygon vertices must be XYZ triples.");
		return false;
	}
	std::vector<glm::dvec2> converted;
	converted.reserve(static_cast<size_t>(ecefComponents.size() / 3));
	for (int32_t index = 0; index < ecefComponents.size(); index += 3) {
		const glm::dvec3 ecef(
			ecefComponents[index],
			ecefComponents[index + 1],
			ecefComponents[index + 2]
		);
		if (!std::isfinite(ecef.x) || !std::isfinite(ecef.y) ||
			!std::isfinite(ecef.z)) {
			ERR_PRINT("ECEF polygon vertices must contain only finite values.");
			return false;
		}
		const std::optional<CesiumGeospatial::Cartographic> cartographic =
			ellipsoid->get_native_ellipsoid().cartesianToCartographic(ecef);
		if (!cartographic.has_value()) {
			ERR_PRINT("An ECEF polygon vertex could not be projected to the ellipsoid.");
			return false;
		}
		converted.emplace_back(cartographic->longitude, cartographic->latitude);
	}
	this->set_vertices_radians(std::move(converted));
	return true;
}

int32_t CesiumCartographicPolygon::get_vertex_count() const {
	return static_cast<int32_t>(this->m_verticesRadians.size());
}

bool CesiumCartographicPolygon::is_valid() const {
	if (this->m_verticesRadians.size() < 3) {
		return false;
	}
	return this->create_native_polygon().getIndices().size() >= 3;
}

PackedInt32Array CesiumCartographicPolygon::get_triangulated_indices() const {
	const std::vector<uint32_t>& indices =
		this->create_native_polygon().getIndices();
	PackedInt32Array result;
	result.resize(static_cast<int32_t>(indices.size()));
	for (int32_t index = 0; index < result.size(); ++index) {
		result.set(index, static_cast<int32_t>(indices[static_cast<size_t>(index)]));
	}
	return result;
}

PackedFloat64Array
CesiumCartographicPolygon::get_bounding_rectangle_degrees() const {
	const std::optional<CesiumGeospatial::GlobeRectangle>& rectangle =
		this->create_native_polygon().getBoundingRectangle();
	if (!rectangle.has_value()) {
		return PackedFloat64Array();
	}
	PackedFloat64Array result;
	result.resize(4);
	result.set(0, Math::rad_to_deg(rectangle->getWest()));
	result.set(1, Math::rad_to_deg(rectangle->getSouth()));
	result.set(2, Math::rad_to_deg(rectangle->getEast()));
	result.set(3, Math::rad_to_deg(rectangle->getNorth()));
	return result;
}

bool CesiumCartographicPolygon::rectangle_is_completely_inside_degrees(
	const PackedFloat64Array& westSouthEastNorth
) const {
	CesiumGeospatial::GlobeRectangle rectangle(0.0, 0.0, 0.0, 0.0);
	if (!unpack_rectangle(westSouthEastNorth, &rectangle)) {
		return false;
	}
	const std::vector<CesiumGeospatial::CartographicPolygon> polygons{
		this->create_native_polygon()
	};
	return CesiumGeospatial::CartographicPolygon::rectangleIsWithinPolygons(
		rectangle,
		polygons
	);
}

bool CesiumCartographicPolygon::rectangle_is_completely_outside_degrees(
	const PackedFloat64Array& westSouthEastNorth
) const {
	CesiumGeospatial::GlobeRectangle rectangle(0.0, 0.0, 0.0, 0.0);
	if (!unpack_rectangle(westSouthEastNorth, &rectangle)) {
		return false;
	}
	const std::vector<CesiumGeospatial::CartographicPolygon> polygons{
		this->create_native_polygon()
	};
	return CesiumGeospatial::CartographicPolygon::rectangleIsOutsidePolygons(
		rectangle,
		polygons
	);
}

CesiumGeospatial::CartographicPolygon
CesiumCartographicPolygon::create_native_polygon() const {
	return CesiumGeospatial::CartographicPolygon(this->m_verticesRadians);
}

void CesiumCartographicPolygon::set_vertices_radians(
	std::vector<glm::dvec2>&& vertices
) {
	if (vertices_equal(this->m_verticesRadians, vertices)) {
		return;
	}
	this->m_verticesRadians = std::move(vertices);
	this->emit_changed();
}

void CesiumCartographicPolygon::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_vertices_degrees", "vertices"), &CesiumCartographicPolygon::set_vertices_degrees);
	ClassDB::bind_method(D_METHOD("get_vertices_degrees"), &CesiumCartographicPolygon::get_vertices_degrees);
	ClassDB::bind_method(D_METHOD("set_vertices_degrees_exact", "components"), &CesiumCartographicPolygon::set_vertices_degrees_exact);
	ClassDB::bind_method(D_METHOD("get_vertices_degrees_exact"), &CesiumCartographicPolygon::get_vertices_degrees_exact);
	ClassDB::bind_method(D_METHOD("set_vertices_from_ecef_exact", "ecef_components", "ellipsoid"), &CesiumCartographicPolygon::set_vertices_from_ecef_exact);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &CesiumCartographicPolygon::get_vertex_count);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumCartographicPolygon::is_valid);
	ClassDB::bind_method(D_METHOD("get_triangulated_indices"), &CesiumCartographicPolygon::get_triangulated_indices);
	ClassDB::bind_method(D_METHOD("get_bounding_rectangle_degrees"), &CesiumCartographicPolygon::get_bounding_rectangle_degrees);
	ClassDB::bind_method(D_METHOD("rectangle_is_completely_inside_degrees", "west_south_east_north"), &CesiumCartographicPolygon::rectangle_is_completely_inside_degrees);
	ClassDB::bind_method(D_METHOD("rectangle_is_completely_outside_degrees", "west_south_east_north"), &CesiumCartographicPolygon::rectangle_is_completely_outside_degrees);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "vertices_degrees"), "set_vertices_degrees", "get_vertices_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "vertices_degrees_exact", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_vertices_degrees_exact", "get_vertices_degrees_exact");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertex_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_vertex_count");
}
