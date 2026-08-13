// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumEllipsoid.h
// - Source/CesiumRuntime/Private/CesiumEllipsoid.cpp
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_ELLIPSOID_H
#define CESIUM_ELLIPSOID_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/variant/packed_float64_array.h"
#include "core/math/vector3.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <CesiumGeospatial/Ellipsoid.h>
#include <glm/vec3.hpp>
#include <cstdint>

/**
 * A reusable, editable ellipsoid backed by Cesium Native's double-precision
 * geospatial math.
 *
 * Vector3 conveniences are provided for ordinary Godot scenes. The `*_precise`
 * APIs use PackedFloat64Array triples so ECEF coordinates remain exact even in
 * a single-precision Godot build.
 */
class CesiumEllipsoid : public Resource {
	GDCLASS(CesiumEllipsoid, Resource)

public:
	CesiumEllipsoid();

	void set_radii(const Vector3& radii);
	Vector3 get_radii() const;
	void set_radii_precise(const PackedFloat64Array& radii);
	PackedFloat64Array get_radii_precise() const;

	double get_maximum_radius() const;
	double get_minimum_radius() const;
	bool is_valid() const;
	int64_t get_revision() const;

	PackedFloat64Array longitude_latitude_height_to_ecef_precise(
		const PackedFloat64Array& longitudeLatitudeHeight
	) const;
	PackedFloat64Array ecef_to_longitude_latitude_height_precise(
		const PackedFloat64Array& ecef
	) const;
	PackedFloat64Array scale_to_geodetic_surface_precise(
		const PackedFloat64Array& ecef
	) const;
	PackedFloat64Array geodetic_surface_normal_precise(
		const PackedFloat64Array& ecef
	) const;
	PackedFloat64Array ray_intersection_precise(
		const PackedFloat64Array& originEcef,
		const PackedFloat64Array& directionEcef
	) const;

	Vector3 longitude_latitude_height_to_ecef(const Vector3& value) const;
	Vector3 ecef_to_longitude_latitude_height(const Vector3& value) const;
	Vector3 scale_to_geodetic_surface(const Vector3& value) const;
	Vector3 geodetic_surface_normal(const Vector3& value) const;

	const CesiumGeospatial::Ellipsoid& get_native_ellipsoid() const;

protected:
	static void _bind_methods();

private:
	void update_native();

	glm::dvec3 m_radii{6378137.0, 6378137.0, 6356752.3142451793};
	CesiumGeospatial::Ellipsoid m_native{
		6378137.0,
		6378137.0,
		6356752.3142451793
	};
	uint64_t m_revision = 1;
};

#endif
