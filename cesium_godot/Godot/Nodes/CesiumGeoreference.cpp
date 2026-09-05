// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Godot/Nodes/CesiumGeoreference.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Runtime/Public/Georeference/CesiumGlobeAnchor.h"
#include "Utils/CesiumMathUtils.h"

#include <CesiumGeometry/IntersectionTests.h>
#include <CesiumGeometry/Ray.h>
#include <CesiumGeospatial/Cartographic.h>

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/camera3d.hpp"
#include "godot_cpp/classes/viewport.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/math.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
constexpr double MINIMUM_SCALE = 1.0e-9;

bool unpack_triple(const PackedFloat64Array& values, glm::dvec3* result) {
	if (values.size() != 3) {
		ERR_PRINT("A precise geospatial coordinate must contain exactly three float64 values.");
		return false;
	}
	const glm::dvec3 candidate(values[0], values[1], values[2]);
	if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) ||
		!std::isfinite(candidate.z)) {
		ERR_PRINT("A precise geospatial coordinate must contain only finite values.");
		return false;
	}
	*result = candidate;
	return true;
}

PackedFloat64Array pack_triple(const glm::dvec3& value) {
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, value.x);
	result.set(1, value.y);
	result.set(2, value.z);
	return result;
}

Vector3 to_vector3(const glm::dvec3& value) {
	return Vector3(value.x, value.y, value.z);
}

glm::dvec3 to_dvec3(const Vector3& value) {
	return glm::dvec3(value.x, value.y, value.z);
}

template <typename T>
void add_object_once(std::vector<ObjectID>& objects, T* object) {
	if (object == nullptr) {
		return;
	}
	const ObjectID id(object->get_instance_id());
	if (std::find(objects.begin(), objects.end(), id) == objects.end()) {
		objects.push_back(id);
	}
}

template <typename T>
void remove_object(std::vector<ObjectID>& objects, T* object) {
	if (object == nullptr) {
		return;
	}
	const ObjectID id(object->get_instance_id());
	objects.erase(std::remove(objects.begin(), objects.end(), id), objects.end());
}
}

CesiumGeoreference::CesiumGeoreference() {
	const glm::dvec3 origin = this->get_native_ellipsoid()
		.cartographicToCartesian(CesiumGeospatial::Cartographic(
			Math::deg_to_rad(this->m_longitude),
			Math::deg_to_rad(this->m_latitude),
			this->m_height
		));
	this->m_ecefPosition = origin;
	this->m_originalEcefPosition = origin;
	this->update_coordinate_system(false);
}

CesiumGeoreference::~CesiumGeoreference() {
	this->disconnect_ellipsoid();
}

void CesiumGeoreference::_enter_tree() {
	if (this->m_ellipsoid.is_null()) this->m_ellipsoid.instantiate();
	this->m_insideTree = true;
	this->m_initialOriginTransform = this->get_global_transform();
	this->m_originalEcefPosition = this->m_ecefPosition;
	if (this->m_ellipsoid.is_valid()) {
		const Callable callback(this, "_on_ellipsoid_resource_changed");
		if (!this->m_ellipsoid->is_connected("changed", callback)) {
			this->m_ellipsoid->connect("changed", callback);
		}
	}
	this->update_coordinate_system(true);
}

void CesiumGeoreference::_exit_tree() {
	this->m_insideTree = false;
	this->m_trackedTilesets.clear();
	this->m_trackedAnchors.clear();
}

void CesiumGeoreference::disconnect_ellipsoid() {
	if (this->m_ellipsoid.is_null()) {
		return;
	}
	const Callable callback(this, "_on_ellipsoid_resource_changed");
	if (this->m_ellipsoid->is_connected("changed", callback)) {
		this->m_ellipsoid->disconnect("changed", callback);
	}
}

void CesiumGeoreference::set_ellipsoid(
	const Ref<CesiumEllipsoid>& ellipsoid
) {
	Ref<CesiumEllipsoid> replacement = ellipsoid;
	if (replacement.is_null()) {
		replacement.instantiate();
	}
	if (this->m_ellipsoid == replacement) {
		return;
	}
	this->disconnect_ellipsoid();
	this->m_ellipsoid = replacement;
	if (this->m_insideTree) {
		this->m_ellipsoid->connect(
			"changed",
			Callable(this, "_on_ellipsoid_resource_changed")
		);
	}
	this->m_ecefPosition = this->get_native_ellipsoid().cartographicToCartesian(
		CesiumGeospatial::Cartographic(
			Math::deg_to_rad(this->m_longitude),
			Math::deg_to_rad(this->m_latitude),
			this->m_height
		)
	);
	this->update_coordinate_system(true);
	this->emit_signal("ellipsoid_changed", this->m_ellipsoid);
}

Ref<CesiumEllipsoid> CesiumGeoreference::get_ellipsoid() const {
	return this->m_ellipsoid;
}

int32_t CesiumGeoreference::get_origin_type() const {
	return static_cast<int32_t>(this->m_originType);
}

CesiumGeoreference::OriginType CesiumGeoreference::get_origin_type_raw() const {
	return this->m_originType;
}

void CesiumGeoreference::set_origin_type(int32_t type) {
	const OriginType bounded = type == TrueOrigin ? TrueOrigin : CartographicOrigin;
	if (this->m_originType == bounded) {
		return;
	}
	this->m_originType = bounded;
	this->update_coordinate_system(true);
}

double CesiumGeoreference::get_ecef_x() const { return this->m_ecefPosition.x; }
double CesiumGeoreference::get_ecef_y() const { return this->m_ecefPosition.y; }
double CesiumGeoreference::get_ecef_z() const { return this->m_ecefPosition.z; }

void CesiumGeoreference::set_ecef_x(double value) {
	glm::dvec3 ecef = this->m_ecefPosition;
	ecef.x = value;
	this->set_origin_ecef_precise(pack_triple(ecef));
}

void CesiumGeoreference::set_ecef_y(double value) {
	glm::dvec3 ecef = this->m_ecefPosition;
	ecef.y = value;
	this->set_origin_ecef_precise(pack_triple(ecef));
}

void CesiumGeoreference::set_ecef_z(double value) {
	glm::dvec3 ecef = this->m_ecefPosition;
	ecef.z = value;
	this->set_origin_ecef_precise(pack_triple(ecef));
}

PackedFloat64Array CesiumGeoreference::get_origin_ecef_precise() const {
	return pack_triple(this->m_ecefPosition);
}

bool CesiumGeoreference::set_origin_ecef_precise(
	const PackedFloat64Array& value
) {
	glm::dvec3 ecef;
	if (!unpack_triple(value, &ecef)) {
		return false;
	}
	const std::optional<CesiumGeospatial::Cartographic> cartographic =
		this->get_native_ellipsoid().cartesianToCartographic(ecef);
	if (!cartographic.has_value()) {
		ERR_PRINT("The georeference origin cannot be placed at the ellipsoid center.");
		return false;
	}
	this->m_ecefPosition = ecef;
	this->m_longitude = Math::rad_to_deg(cartographic->longitude);
	this->m_latitude = Math::rad_to_deg(cartographic->latitude);
	this->m_height = cartographic->height;
	this->update_coordinate_system(true);
	return true;
}

double CesiumGeoreference::get_latitude() const { return this->m_latitude; }
double CesiumGeoreference::get_longitude() const { return this->m_longitude; }
double CesiumGeoreference::get_altitude() const { return this->m_height; }

void CesiumGeoreference::set_latitude(double value) {
	this->m_latitude = Math::clamp(value, -90.0, 90.0);
	this->update_ecef_with_lla(this->get_lla());
}

void CesiumGeoreference::set_longitude(double value) {
	this->m_longitude = value;
	this->update_ecef_with_lla(this->get_lla());
}

void CesiumGeoreference::set_altitude(double value) {
	this->m_height = value;
	this->update_ecef_with_lla(this->get_lla());
}

PackedFloat64Array
CesiumGeoreference::get_origin_longitude_latitude_height_precise() const {
	return pack_triple(glm::dvec3(
		this->m_longitude,
		this->m_latitude,
		this->m_height
	));
}

void CesiumGeoreference::set_origin_longitude_latitude_height_precise(
	const PackedFloat64Array& value
) {
	glm::dvec3 llh;
	if (!unpack_triple(value, &llh)) {
		return;
	}
	this->m_longitude = llh.x;
	this->m_latitude = Math::clamp(llh.y, -90.0, 90.0);
	this->m_height = llh.z;
	this->update_ecef_with_lla(this->get_lla());
}

real_t CesiumGeoreference::get_scale_factor() const {
	return this->m_scaleFactor;
}

void CesiumGeoreference::set_scale_factor(real_t value) {
	const real_t bounded = Math::max<real_t>(value, MINIMUM_SCALE);
	if (Math::is_equal_approx(this->m_scaleFactor, bounded)) {
		return;
	}
	this->m_scaleFactor = bounded;
	this->update_coordinate_system(true);
}

int64_t CesiumGeoreference::get_revision() const {
	return static_cast<int64_t>(this->m_revision);
}

PackedFloat64Array CesiumGeoreference::transform_ecef_position_to_local_precise(
	const PackedFloat64Array& ecef
) const {
	glm::dvec3 value;
	return unpack_triple(ecef, &value)
		? pack_triple(this->m_coordinateSystem.ecefPositionToLocal(value))
		: PackedFloat64Array();
}

PackedFloat64Array CesiumGeoreference::transform_local_position_to_ecef_precise(
	const PackedFloat64Array& local
) const {
	glm::dvec3 value;
	return unpack_triple(local, &value)
		? pack_triple(this->m_coordinateSystem.localPositionToEcef(value))
		: PackedFloat64Array();
}

PackedFloat64Array CesiumGeoreference::transform_ecef_direction_to_local_precise(
	const PackedFloat64Array& ecef
) const {
	glm::dvec3 value;
	return unpack_triple(ecef, &value)
		? pack_triple(this->m_coordinateSystem.ecefDirectionToLocal(value))
		: PackedFloat64Array();
}

PackedFloat64Array CesiumGeoreference::transform_local_direction_to_ecef_precise(
	const PackedFloat64Array& local
) const {
	glm::dvec3 value;
	return unpack_triple(local, &value)
		? pack_triple(this->m_coordinateSystem.localDirectionToEcef(value))
		: PackedFloat64Array();
}

PackedFloat64Array
CesiumGeoreference::transform_longitude_latitude_height_to_local_precise(
	const PackedFloat64Array& longitudeLatitudeHeight
) const {
	const PackedFloat64Array ecef = this->m_ellipsoid
		->longitude_latitude_height_to_ecef_precise(longitudeLatitudeHeight);
	return ecef.size() == 3
		? this->transform_ecef_position_to_local_precise(ecef)
		: PackedFloat64Array();
}

PackedFloat64Array
CesiumGeoreference::transform_local_to_longitude_latitude_height_precise(
	const PackedFloat64Array& local
) const {
	const PackedFloat64Array ecef =
		this->transform_local_position_to_ecef_precise(local);
	return ecef.size() == 3
		? this->m_ellipsoid->ecef_to_longitude_latitude_height_precise(ecef)
		: PackedFloat64Array();
}

Vector3 CesiumGeoreference::transform_ecef_position_to_local(
	const Vector3& ecef
) const {
	return to_vector3(this->m_coordinateSystem.ecefPositionToLocal(to_dvec3(ecef)));
}

Vector3 CesiumGeoreference::transform_local_position_to_ecef(
	const Vector3& local
) const {
	return to_vector3(this->m_coordinateSystem.localPositionToEcef(to_dvec3(local)));
}

Vector3 CesiumGeoreference::transform_ecef_direction_to_local(
	const Vector3& ecef
) const {
	return to_vector3(this->m_coordinateSystem.ecefDirectionToLocal(to_dvec3(ecef)));
}

Vector3 CesiumGeoreference::transform_local_direction_to_ecef(
	const Vector3& local
) const {
	return to_vector3(this->m_coordinateSystem.localDirectionToEcef(to_dvec3(local)));
}

Vector3 CesiumGeoreference::transform_global_position_to_ecef(
	const Vector3& global
) const {
	const Vector3 local = this->get_global_transform().affine_inverse().xform(global);
	return this->transform_local_position_to_ecef(local);
}

Vector3 CesiumGeoreference::transform_ecef_position_to_global(
	const Vector3& ecef
) const {
	return this->get_global_transform().xform(
		this->transform_ecef_position_to_local(ecef)
	);
}

const CesiumGeospatial::LocalHorizontalCoordinateSystem&
CesiumGeoreference::get_coordinate_system() const {
	return this->m_coordinateSystem;
}

const CesiumGeospatial::Ellipsoid& CesiumGeoreference::get_native_ellipsoid() const {
	return this->m_ellipsoid.is_valid()
		? this->m_ellipsoid->get_native_ellipsoid()
		: CesiumGeospatial::Ellipsoid::WGS84;
}

const glm::dvec3& CesiumGeoreference::get_ecef_position() const {
	return this->m_ecefPosition;
}

glm::dvec3 CesiumGeoreference::get_lla() const {
	// Compatibility ordering is latitude, longitude, altitude.
	return glm::dvec3(this->m_latitude, this->m_longitude, this->m_height);
}

glm::dmat4 CesiumGeoreference::ecef_transform_to_local(
	const glm::dmat4& transform
) const {
	return this->m_coordinateSystem.getEcefToLocalTransformation() * transform;
}

glm::dmat4 CesiumGeoreference::local_transform_to_ecef(
	const glm::dmat4& transform
) const {
	return this->m_coordinateSystem.getLocalToEcefTransformation() * transform;
}

Transform3D CesiumGeoreference::get_tx_engine_to_ecef() const {
	return this->get_tx_ecef_to_engine().affine_inverse();
}

Transform3D CesiumGeoreference::get_tx_ecef_to_engine() const {
	return this->get_global_transform() * CesiumMathUtils::from_glm_mat4(
		this->m_coordinateSystem.getEcefToLocalTransformation()
	);
}

Transform3D CesiumGeoreference::get_initial_tx_ecef_to_engine() const {
	return this->m_initialOriginTransform * CesiumMathUtils::from_glm_mat4(
		this->m_coordinateSystem.getEcefToLocalTransformation()
	);
}

Transform3D CesiumGeoreference::get_initial_tx_engine_to_ecef() const {
	return this->get_initial_tx_ecef_to_engine().affine_inverse();
}

Vector3 CesiumGeoreference::get_global_surface_position(
	const Vector3& cameraPosition,
	const Vector3& cameraDirection
) {
	return this->ray_to_surface(cameraPosition, cameraDirection);
}

Vector3 CesiumGeoreference::get_global_center_position() {
	return this->get_global_position();
}

PackedFloat64Array CesiumGeoreference::get_mouse_pos_ecef_precise() {
	Viewport* viewport = this->get_viewport();
	if (viewport == nullptr || viewport->get_camera_3d() == nullptr) {
		return PackedFloat64Array();
	}
	Camera3D* camera = viewport->get_camera_3d();
	const Vector3 globalOrigin = camera->project_ray_origin(
		viewport->get_mouse_position()
	);
	const Vector3 globalDirection = camera->project_ray_normal(
		viewport->get_mouse_position()
	);
	const Transform3D globalToLocal = this->get_global_transform().affine_inverse();
	const glm::dvec3 localOrigin = to_dvec3(globalToLocal.xform(globalOrigin));
	const glm::dvec3 localDirection = to_dvec3(
		globalToLocal.basis.xform(globalDirection)
	);
	const std::optional<glm::dvec3> hit = this->intersect_ellipsoid(
		this->m_coordinateSystem.localPositionToEcef(localOrigin),
		this->m_coordinateSystem.localDirectionToEcef(localDirection)
	);
	return hit.has_value() ? pack_triple(*hit) : PackedFloat64Array();
}

Vector3 CesiumGeoreference::get_mouse_pos_ecef() {
	const PackedFloat64Array hit = this->get_mouse_pos_ecef_precise();
	return hit.size() == 3 ? Vector3(hit[0], hit[1], hit[2]) : Vector3(NAN, NAN, NAN);
}

Vector3 CesiumGeoreference::get_ellipsoid_dimensions() const {
	return this->m_ellipsoid->get_radii();
}

std::optional<glm::dvec3> CesiumGeoreference::intersect_ellipsoid(
	const glm::dvec3& origin,
	const glm::dvec3& direction
) const {
	const double length = glm::length(direction);
	if (!std::isfinite(length) || length <= MINIMUM_SCALE) {
		return std::nullopt;
	}
	const CesiumGeometry::Ray ray(origin, direction / length);
	const std::optional<glm::dvec2> interval =
		CesiumGeometry::IntersectionTests::rayEllipsoid(
			ray,
			this->get_native_ellipsoid().getRadii()
		);
	if (!interval.has_value()) {
		return std::nullopt;
	}
	const double distance = interval->x >= 0.0 ? interval->x : interval->y;
	return distance >= 0.0
		? std::optional<glm::dvec3>(ray.pointFromDistance(distance))
		: std::nullopt;
}

Vector3 CesiumGeoreference::ray_to_surface(
	const Vector3& globalOrigin,
	const Vector3& globalDirection
) const {
	const Transform3D globalToLocal = this->get_global_transform().affine_inverse();
	const glm::dvec3 originEcef = this->m_coordinateSystem.localPositionToEcef(
		to_dvec3(globalToLocal.xform(globalOrigin))
	);
	const glm::dvec3 directionEcef = this->m_coordinateSystem.localDirectionToEcef(
		to_dvec3(globalToLocal.basis.xform(globalDirection))
	);
	const std::optional<glm::dvec3> hit =
		this->intersect_ellipsoid(originEcef, directionEcef);
	if (!hit.has_value()) {
		return Vector3(NAN, NAN, NAN);
	}
	const Vector3 localHit = to_vector3(
		this->m_coordinateSystem.ecefPositionToLocal(*hit)
	);
	return this->get_global_transform().xform(localHit);
}

Basis CesiumGeoreference::eus_at_ecef(const Vector3& ecef) const {
	const glm::dvec3 position = to_dvec3(ecef);
	const glm::dvec3 up = this->get_native_ellipsoid().geodeticSurfaceNormal(position);
	glm::dvec3 east = glm::cross(glm::dvec3(0.0, 0.0, 1.0), up);
	if (glm::length(east) <= MINIMUM_SCALE) {
		east = glm::dvec3(0.0, 1.0, 0.0);
	} else {
		east = glm::normalize(east);
	}
	const glm::dvec3 south = glm::cross(east, up);
	return Basis(
		to_vector3(this->m_coordinateSystem.ecefDirectionToLocal(east)).normalized(),
		to_vector3(this->m_coordinateSystem.ecefDirectionToLocal(up)).normalized(),
		to_vector3(this->m_coordinateSystem.ecefDirectionToLocal(south)).normalized()
	);
}

Vector3 CesiumGeoreference::get_normal_at_surface_pos(const Vector3& ecef) const {
	const glm::dvec3 normal = this->get_native_ellipsoid().geodeticSurfaceNormal(
		to_dvec3(ecef)
	);
	const Vector3 local = to_vector3(
		this->m_coordinateSystem.ecefDirectionToLocal(normal)
	).normalized();
	return this->get_global_transform().basis.xform(local).normalized();
}

Vector3 CesiumGeoreference::ecef_to_lat_lon_alt_rad(const Vector3& ecef) const {
	const std::optional<CesiumGeospatial::Cartographic> value =
		this->get_native_ellipsoid().cartesianToCartographic(to_dvec3(ecef));
	return value.has_value()
		? Vector3(value->latitude, value->longitude, value->height)
		: Vector3();
}

Vector3 CesiumGeoreference::ecef_to_lat_lon_alt_deg(const Vector3& ecef) const {
	const Vector3 radians = this->ecef_to_lat_lon_alt_rad(ecef);
	return Vector3(
		Math::rad_to_deg(radians.x),
		Math::rad_to_deg(radians.y),
		radians.z
	);
}

Vector3 CesiumGeoreference::lat_lon_alt_rad_to_ecef(
	const Vector3& latitudeLongitudeAltitude
) const {
	return to_vector3(this->get_native_ellipsoid().cartographicToCartesian(
		CesiumGeospatial::Cartographic(
			latitudeLongitudeAltitude.y,
			latitudeLongitudeAltitude.x,
			latitudeLongitudeAltitude.z
		)
	));
}

void CesiumGeoreference::update_ecef_with_lla(
	const glm::dvec3& latitudeLongitudeAltitude
) {
	this->m_latitude = Math::clamp(latitudeLongitudeAltitude.x, -90.0, 90.0);
	this->m_longitude = latitudeLongitudeAltitude.y;
	this->m_height = latitudeLongitudeAltitude.z;
	this->m_ecefPosition = this->get_native_ellipsoid().cartographicToCartesian(
		CesiumGeospatial::Cartographic(
			Math::deg_to_rad(this->m_longitude),
			Math::deg_to_rad(this->m_latitude),
			this->m_height
		)
	);
	this->update_coordinate_system(true);
}

const glm::dvec3& CesiumGeoreference::get_original_origin_ecef() const {
	return this->m_originalEcefPosition;
}

void CesiumGeoreference::set_should_update_origin(bool value) {
	this->m_shouldUpdateOrigin = value;
}

bool CesiumGeoreference::get_should_update_origin() const {
	return this->m_shouldUpdateOrigin;
}

void CesiumGeoreference::register_tileset_to_move_origin(
	Cesium3DTileset* tileset
) {
	add_object_once(this->m_trackedTilesets, tileset);
}

void CesiumGeoreference::unregister_tileset_to_move_origin(
	Cesium3DTileset* tileset
) {
	remove_object(this->m_trackedTilesets, tileset);
}

void CesiumGeoreference::register_globe_anchor(CesiumGlobeAnchor* anchor) {
	add_object_once(this->m_trackedAnchors, anchor);
}

void CesiumGeoreference::unregister_globe_anchor(CesiumGlobeAnchor* anchor) {
	remove_object(this->m_trackedAnchors, anchor);
}

void CesiumGeoreference::refresh_dependents() {
	this->m_trackedTilesets.erase(
		std::remove_if(
			this->m_trackedTilesets.begin(),
			this->m_trackedTilesets.end(),
			[](const ObjectID& id) {
				Cesium3DTileset* tileset = Object::cast_to<Cesium3DTileset>(
					ObjectDB::get_instance(id)
				);
				if (tileset == nullptr) {
					return true;
				}
				tileset->apply_georeference();
				return false;
			}
		),
		this->m_trackedTilesets.end()
	);
	this->m_trackedAnchors.erase(
		std::remove_if(
			this->m_trackedAnchors.begin(),
			this->m_trackedAnchors.end(),
			[](const ObjectID& id) {
				CesiumGlobeAnchor* anchor = Object::cast_to<CesiumGlobeAnchor>(
					ObjectDB::get_instance(id)
				);
				if (anchor == nullptr) {
					return true;
				}
				anchor->handle_georeference_changed();
				return false;
			}
		),
		this->m_trackedAnchors.end()
	);
}

void CesiumGeoreference::_on_ellipsoid_resource_changed() {
	// Match Cesium for Unreal: the authored cartographic origin is stable when
	// its ellipsoid asset changes, while the derived ECEF center is recomputed.
	this->m_ecefPosition = this->get_native_ellipsoid().cartographicToCartesian(
		CesiumGeospatial::Cartographic(
			Math::deg_to_rad(this->m_longitude),
			Math::deg_to_rad(this->m_latitude),
			this->m_height
		)
	);
	this->update_coordinate_system(true);
}

void CesiumGeoreference::update_coordinate_system(bool notifyDependents) {
	const double scaleToMeters = 1.0 / Math::max<double>(
		static_cast<double>(this->m_scaleFactor),
		MINIMUM_SCALE
	);
	if (this->m_originType == CartographicOrigin) {
		this->m_coordinateSystem =
			CesiumGeospatial::LocalHorizontalCoordinateSystem(
				this->m_ecefPosition,
				CesiumGeospatial::LocalDirection::East,
				CesiumGeospatial::LocalDirection::Up,
				CesiumGeospatial::LocalDirection::South,
				scaleToMeters,
				this->get_native_ellipsoid()
			);
	} else {
		// Godot X/Y/Z map to Cesium X/Z/-Y. The local frame stays right-handed,
		// Godot Y is up, and Godot -Z is the native +Y forward direction.
		const glm::dmat4 localToEcef(
			glm::dvec4(scaleToMeters, 0.0, 0.0, 0.0),
			glm::dvec4(0.0, 0.0, scaleToMeters, 0.0),
			glm::dvec4(0.0, -scaleToMeters, 0.0, 0.0),
			glm::dvec4(0.0, 0.0, 0.0, 1.0)
		);
		this->m_coordinateSystem =
			CesiumGeospatial::LocalHorizontalCoordinateSystem(localToEcef);
	}
	++this->m_revision;
	if (notifyDependents) {
		this->refresh_dependents();
		this->emit_signal("georeference_changed", static_cast<int64_t>(this->m_revision));
	}
}

void CesiumGeoreference::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ellipsoid", "ellipsoid"), &CesiumGeoreference::set_ellipsoid);
	ClassDB::bind_method(D_METHOD("get_ellipsoid"), &CesiumGeoreference::get_ellipsoid);
	ClassDB::bind_method(D_METHOD("get_origin_type"), &CesiumGeoreference::get_origin_type);
	ClassDB::bind_method(D_METHOD("set_origin_type", "origin_type"), &CesiumGeoreference::set_origin_type);
	ClassDB::bind_method(D_METHOD("get_ecef_x"), &CesiumGeoreference::get_ecef_x);
	ClassDB::bind_method(D_METHOD("set_ecef_x", "value"), &CesiumGeoreference::set_ecef_x);
	ClassDB::bind_method(D_METHOD("get_ecef_y"), &CesiumGeoreference::get_ecef_y);
	ClassDB::bind_method(D_METHOD("set_ecef_y", "value"), &CesiumGeoreference::set_ecef_y);
	ClassDB::bind_method(D_METHOD("get_ecef_z"), &CesiumGeoreference::get_ecef_z);
	ClassDB::bind_method(D_METHOD("set_ecef_z", "value"), &CesiumGeoreference::set_ecef_z);
	ClassDB::bind_method(D_METHOD("get_origin_ecef_precise"), &CesiumGeoreference::get_origin_ecef_precise);
	ClassDB::bind_method(D_METHOD("set_origin_ecef_precise", "ecef"), &CesiumGeoreference::set_origin_ecef_precise);
	ClassDB::bind_method(D_METHOD("get_latitude"), &CesiumGeoreference::get_latitude);
	ClassDB::bind_method(D_METHOD("set_latitude", "value"), &CesiumGeoreference::set_latitude);
	ClassDB::bind_method(D_METHOD("get_longitude"), &CesiumGeoreference::get_longitude);
	ClassDB::bind_method(D_METHOD("set_longitude", "value"), &CesiumGeoreference::set_longitude);
	ClassDB::bind_method(D_METHOD("get_altitude"), &CesiumGeoreference::get_altitude);
	ClassDB::bind_method(D_METHOD("set_altitude", "value"), &CesiumGeoreference::set_altitude);
	ClassDB::bind_method(D_METHOD("get_origin_longitude_latitude_height_precise"), &CesiumGeoreference::get_origin_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("set_origin_longitude_latitude_height_precise", "longitude_latitude_height"), &CesiumGeoreference::set_origin_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("get_scale_factor"), &CesiumGeoreference::get_scale_factor);
	ClassDB::bind_method(D_METHOD("set_scale_factor", "value"), &CesiumGeoreference::set_scale_factor);
	ClassDB::bind_method(D_METHOD("get_revision"), &CesiumGeoreference::get_revision);
	ClassDB::bind_method(D_METHOD("transform_ecef_position_to_local_precise", "ecef"), &CesiumGeoreference::transform_ecef_position_to_local_precise);
	ClassDB::bind_method(D_METHOD("transform_local_position_to_ecef_precise", "local"), &CesiumGeoreference::transform_local_position_to_ecef_precise);
	ClassDB::bind_method(D_METHOD("transform_ecef_direction_to_local_precise", "ecef"), &CesiumGeoreference::transform_ecef_direction_to_local_precise);
	ClassDB::bind_method(D_METHOD("transform_local_direction_to_ecef_precise", "local"), &CesiumGeoreference::transform_local_direction_to_ecef_precise);
	ClassDB::bind_method(D_METHOD("transform_longitude_latitude_height_to_local_precise", "longitude_latitude_height"), &CesiumGeoreference::transform_longitude_latitude_height_to_local_precise);
	ClassDB::bind_method(D_METHOD("transform_local_to_longitude_latitude_height_precise", "local"), &CesiumGeoreference::transform_local_to_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("transform_ecef_position_to_local", "ecef"), &CesiumGeoreference::transform_ecef_position_to_local);
	ClassDB::bind_method(D_METHOD("transform_local_position_to_ecef", "local"), &CesiumGeoreference::transform_local_position_to_ecef);
	ClassDB::bind_method(D_METHOD("transform_ecef_direction_to_local", "ecef"), &CesiumGeoreference::transform_ecef_direction_to_local);
	ClassDB::bind_method(D_METHOD("transform_local_direction_to_ecef", "local"), &CesiumGeoreference::transform_local_direction_to_ecef);
	ClassDB::bind_method(D_METHOD("transform_global_position_to_ecef", "global"), &CesiumGeoreference::transform_global_position_to_ecef);
	ClassDB::bind_method(D_METHOD("transform_ecef_position_to_global", "ecef"), &CesiumGeoreference::transform_ecef_position_to_global);
	ClassDB::bind_method(D_METHOD("get_tx_engine_to_ecef"), &CesiumGeoreference::get_tx_engine_to_ecef);
	ClassDB::bind_method(D_METHOD("get_tx_ecef_to_engine"), &CesiumGeoreference::get_tx_ecef_to_engine);
	ClassDB::bind_method(D_METHOD("get_initial_tx_engine_to_ecef"), &CesiumGeoreference::get_initial_tx_engine_to_ecef);
	ClassDB::bind_method(D_METHOD("get_initial_tx_ecef_to_engine"), &CesiumGeoreference::get_initial_tx_ecef_to_engine);
	ClassDB::bind_method(D_METHOD("get_global_center_position"), &CesiumGeoreference::get_global_center_position);
	ClassDB::bind_method(D_METHOD("get_global_surface_position", "camera_position", "camera_direction"), &CesiumGeoreference::get_global_surface_position);
	ClassDB::bind_method(D_METHOD("get_mouse_pos_ecef"), &CesiumGeoreference::get_mouse_pos_ecef);
	ClassDB::bind_method(D_METHOD("get_mouse_pos_ecef_precise"), &CesiumGeoreference::get_mouse_pos_ecef_precise);
	ClassDB::bind_method(D_METHOD("ray_to_surface", "origin", "direction"), &CesiumGeoreference::ray_to_surface);
	ClassDB::bind_method(D_METHOD("get_ellipsoid_dimensions"), &CesiumGeoreference::get_ellipsoid_dimensions);
	ClassDB::bind_method(D_METHOD("eus_at_ecef", "ecef"), &CesiumGeoreference::eus_at_ecef);
	ClassDB::bind_method(D_METHOD("get_normal_at_surface_pos", "ecef"), &CesiumGeoreference::get_normal_at_surface_pos);
	ClassDB::bind_method(D_METHOD("ecef_to_lat_lon_alt_deg", "ecef"), &CesiumGeoreference::ecef_to_lat_lon_alt_deg);
	ClassDB::bind_method(D_METHOD("ecef_to_lat_lon_alt_rad", "ecef"), &CesiumGeoreference::ecef_to_lat_lon_alt_rad);
	ClassDB::bind_method(D_METHOD("lat_lon_alt_rad_to_ecef", "latitude_longitude_altitude"), &CesiumGeoreference::lat_lon_alt_rad_to_ecef);
	ClassDB::bind_method(D_METHOD("set_should_update_origin", "value"), &CesiumGeoreference::set_should_update_origin);
	ClassDB::bind_method(D_METHOD("get_should_update_origin"), &CesiumGeoreference::get_should_update_origin);
	ClassDB::bind_method(D_METHOD("refresh_dependents"), &CesiumGeoreference::refresh_dependents);
	ClassDB::bind_method(D_METHOD("_on_ellipsoid_resource_changed"), &CesiumGeoreference::_on_ellipsoid_resource_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ellipsoid", PROPERTY_HINT_RESOURCE_TYPE, "CesiumEllipsoid"), "set_ellipsoid", "get_ellipsoid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "origin_type", PROPERTY_HINT_ENUM, "Cartographic Origin,True Origin"), "set_origin_type", "get_origin_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "latitude", PROPERTY_HINT_RANGE, "-90,90,0.000001"), "set_latitude", "get_latitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "longitude", PROPERTY_HINT_RANGE, "-180,180,0.000001,or_less,or_greater"), "set_longitude", "get_longitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "altitude"), "set_altitude", "get_altitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ecefX"), "set_ecef_x", "get_ecef_x");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ecefY"), "set_ecef_y", "get_ecef_y");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ecefZ"), "set_ecef_z", "get_ecef_z");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_factor", PROPERTY_HINT_RANGE, "0.000001,1000000,0.001,or_greater,exp"), "set_scale_factor", "get_scale_factor");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "should_update_origin", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_should_update_origin", "get_should_update_origin");

	BIND_ENUM_CONSTANT(CartographicOrigin);
	BIND_ENUM_CONSTANT(TrueOrigin);
	ADD_SIGNAL(MethodInfo("georeference_changed", PropertyInfo(Variant::INT, "revision")));
	ADD_SIGNAL(MethodInfo("ellipsoid_changed", PropertyInfo(Variant::OBJECT, "ellipsoid", PROPERTY_HINT_RESOURCE_TYPE, "CesiumEllipsoid")));
}
