// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumEllipsoid.h"

#include <CesiumGeometry/IntersectionTests.h>
#include <CesiumGeometry/Ray.h>
#include <CesiumGeospatial/Cartographic.h>

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/math.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {
constexpr double MINIMUM_RADIUS = 1.0e-9;

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
	return Vector3(
		static_cast<real_t>(value.x),
		static_cast<real_t>(value.y),
		static_cast<real_t>(value.z)
	);
}

glm::dvec3 to_dvec3(const Vector3& value) {
	return glm::dvec3(value.x, value.y, value.z);
}
}

CesiumEllipsoid::CesiumEllipsoid() = default;

void CesiumEllipsoid::set_radii(const Vector3& radii) {
	PackedFloat64Array precise;
	precise.resize(3);
	precise.set(0, radii.x);
	precise.set(1, radii.y);
	precise.set(2, radii.z);
	this->set_radii_precise(precise);
}

Vector3 CesiumEllipsoid::get_radii() const {
	return to_vector3(this->m_radii);
}

void CesiumEllipsoid::set_radii_precise(const PackedFloat64Array& radii) {
	glm::dvec3 candidate;
	if (!unpack_triple(radii, &candidate)) {
		return;
	}
	if (candidate.x <= 0.0 || candidate.y <= 0.0 || candidate.z <= 0.0) {
		ERR_PRINT("Ellipsoid radii must all be greater than zero.");
		return;
	}
	candidate = glm::max(candidate, glm::dvec3(MINIMUM_RADIUS));
	if (candidate == this->m_radii) {
		return;
	}
	this->m_radii = candidate;
	this->update_native();
}

PackedFloat64Array CesiumEllipsoid::get_radii_precise() const {
	return pack_triple(this->m_radii);
}

double CesiumEllipsoid::get_maximum_radius() const {
	return this->m_native.getMaximumRadius();
}

double CesiumEllipsoid::get_minimum_radius() const {
	return this->m_native.getMinimumRadius();
}

bool CesiumEllipsoid::is_valid() const {
	return this->m_radii.x > 0.0 && this->m_radii.y > 0.0 &&
		this->m_radii.z > 0.0 && std::isfinite(this->m_radii.x) &&
		std::isfinite(this->m_radii.y) && std::isfinite(this->m_radii.z);
}

int64_t CesiumEllipsoid::get_revision() const {
	return static_cast<int64_t>(this->m_revision);
}

PackedFloat64Array CesiumEllipsoid::longitude_latitude_height_to_ecef_precise(
	const PackedFloat64Array& longitudeLatitudeHeight
) const {
	glm::dvec3 value;
	if (!unpack_triple(longitudeLatitudeHeight, &value)) {
		return PackedFloat64Array();
	}
	const CesiumGeospatial::Cartographic cartographic(
		Math::deg_to_rad(value.x),
		Math::deg_to_rad(value.y),
		value.z
	);
	return pack_triple(this->m_native.cartographicToCartesian(cartographic));
}

PackedFloat64Array CesiumEllipsoid::ecef_to_longitude_latitude_height_precise(
	const PackedFloat64Array& ecef
) const {
	glm::dvec3 value;
	if (!unpack_triple(ecef, &value)) {
		return PackedFloat64Array();
	}
	const std::optional<CesiumGeospatial::Cartographic> cartographic =
		this->m_native.cartesianToCartographic(value);
	if (!cartographic.has_value()) {
		return PackedFloat64Array();
	}
	return pack_triple(glm::dvec3(
		Math::rad_to_deg(cartographic->longitude),
		Math::rad_to_deg(cartographic->latitude),
		cartographic->height
	));
}

PackedFloat64Array CesiumEllipsoid::scale_to_geodetic_surface_precise(
	const PackedFloat64Array& ecef
) const {
	glm::dvec3 value;
	if (!unpack_triple(ecef, &value)) {
		return PackedFloat64Array();
	}
	const std::optional<glm::dvec3> surface =
		this->m_native.scaleToGeodeticSurface(value);
	return surface.has_value() ? pack_triple(*surface) : PackedFloat64Array();
}

PackedFloat64Array CesiumEllipsoid::geodetic_surface_normal_precise(
	const PackedFloat64Array& ecef
) const {
	glm::dvec3 value;
	if (!unpack_triple(ecef, &value) || glm::length(value) <= MINIMUM_RADIUS) {
		return PackedFloat64Array();
	}
	return pack_triple(this->m_native.geodeticSurfaceNormal(value));
}

PackedFloat64Array CesiumEllipsoid::ray_intersection_precise(
	const PackedFloat64Array& originEcef,
	const PackedFloat64Array& directionEcef
) const {
	glm::dvec3 origin;
	glm::dvec3 direction;
	if (!unpack_triple(originEcef, &origin) ||
		!unpack_triple(directionEcef, &direction)) {
		return PackedFloat64Array();
	}
	const double magnitude = glm::length(direction);
	if (!std::isfinite(magnitude) || magnitude <= MINIMUM_RADIUS) {
		return PackedFloat64Array();
	}
	direction /= magnitude;
	const CesiumGeometry::Ray ray(origin, direction);
	const std::optional<glm::dvec2> interval =
		CesiumGeometry::IntersectionTests::rayEllipsoid(ray, this->m_radii);
	if (!interval.has_value()) {
		return PackedFloat64Array();
	}
	const double distance = interval->x >= 0.0 ? interval->x : interval->y;
	if (distance < 0.0 || !std::isfinite(distance)) {
		return PackedFloat64Array();
	}
	return pack_triple(ray.pointFromDistance(distance));
}

Vector3 CesiumEllipsoid::longitude_latitude_height_to_ecef(
	const Vector3& value
) const {
	const CesiumGeospatial::Cartographic cartographic(
		Math::deg_to_rad(static_cast<double>(value.x)),
		Math::deg_to_rad(static_cast<double>(value.y)),
		value.z
	);
	return to_vector3(this->m_native.cartographicToCartesian(cartographic));
}

Vector3 CesiumEllipsoid::ecef_to_longitude_latitude_height(
	const Vector3& value
) const {
	const std::optional<CesiumGeospatial::Cartographic> cartographic =
		this->m_native.cartesianToCartographic(to_dvec3(value));
	if (!cartographic.has_value()) {
		return Vector3();
	}
	return Vector3(
		Math::rad_to_deg(cartographic->longitude),
		Math::rad_to_deg(cartographic->latitude),
		cartographic->height
	);
}

Vector3 CesiumEllipsoid::scale_to_geodetic_surface(const Vector3& value) const {
	const std::optional<glm::dvec3> surface =
		this->m_native.scaleToGeodeticSurface(to_dvec3(value));
	return surface.has_value() ? to_vector3(*surface) : Vector3();
}

Vector3 CesiumEllipsoid::geodetic_surface_normal(const Vector3& value) const {
	const glm::dvec3 precise = to_dvec3(value);
	return glm::length(precise) > MINIMUM_RADIUS
		? to_vector3(this->m_native.geodeticSurfaceNormal(precise))
		: Vector3();
}

const CesiumGeospatial::Ellipsoid& CesiumEllipsoid::get_native_ellipsoid() const {
	return this->m_native;
}

void CesiumEllipsoid::update_native() {
	this->m_native = CesiumGeospatial::Ellipsoid(this->m_radii);
	++this->m_revision;
	this->emit_changed();
}

void CesiumEllipsoid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radii", "radii"), &CesiumEllipsoid::set_radii);
	ClassDB::bind_method(D_METHOD("get_radii"), &CesiumEllipsoid::get_radii);
	ClassDB::bind_method(D_METHOD("set_radii_precise", "radii"), &CesiumEllipsoid::set_radii_precise);
	ClassDB::bind_method(D_METHOD("get_radii_precise"), &CesiumEllipsoid::get_radii_precise);
	ClassDB::bind_method(D_METHOD("get_maximum_radius"), &CesiumEllipsoid::get_maximum_radius);
	ClassDB::bind_method(D_METHOD("get_minimum_radius"), &CesiumEllipsoid::get_minimum_radius);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumEllipsoid::is_valid);
	ClassDB::bind_method(D_METHOD("get_revision"), &CesiumEllipsoid::get_revision);
	ClassDB::bind_method(D_METHOD("longitude_latitude_height_to_ecef_precise", "longitude_latitude_height"), &CesiumEllipsoid::longitude_latitude_height_to_ecef_precise);
	ClassDB::bind_method(D_METHOD("ecef_to_longitude_latitude_height_precise", "ecef"), &CesiumEllipsoid::ecef_to_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("scale_to_geodetic_surface_precise", "ecef"), &CesiumEllipsoid::scale_to_geodetic_surface_precise);
	ClassDB::bind_method(D_METHOD("geodetic_surface_normal_precise", "ecef"), &CesiumEllipsoid::geodetic_surface_normal_precise);
	ClassDB::bind_method(D_METHOD("ray_intersection_precise", "origin_ecef", "direction_ecef"), &CesiumEllipsoid::ray_intersection_precise);
	ClassDB::bind_method(D_METHOD("longitude_latitude_height_to_ecef", "longitude_latitude_height"), &CesiumEllipsoid::longitude_latitude_height_to_ecef);
	ClassDB::bind_method(D_METHOD("ecef_to_longitude_latitude_height", "ecef"), &CesiumEllipsoid::ecef_to_longitude_latitude_height);
	ClassDB::bind_method(D_METHOD("scale_to_geodetic_surface", "ecef"), &CesiumEllipsoid::scale_to_geodetic_surface);
	ClassDB::bind_method(D_METHOD("geodetic_surface_normal", "ecef"), &CesiumEllipsoid::geodetic_surface_normal);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "radii", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_radii", "get_radii");
}
