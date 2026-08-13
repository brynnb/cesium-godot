// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterpart: Source/CesiumRuntime/Public/CesiumCartographicPolygon.h
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_CARTOGRAPHIC_POLYGON_H
#define CESIUM_CARTOGRAPHIC_POLYGON_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/variant/packed_float64_array.h"
#include "core/variant/packed_int32_array.h"
#include "core/variant/packed_vector2_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
using namespace godot;
#endif

#include <CesiumGeospatial/CartographicPolygon.h>

#include <cstdint>
#include <vector>

class CesiumEllipsoid;

/**
 * A reusable polygon whose vertices are longitude/latitude pairs in degrees.
 *
 * `vertices_degrees_exact` stores alternating float64 longitude and latitude
 * values. It is the authoritative representation; the PackedVector2Array API
 * is an inspector-friendly convenience that may be lossy in a single build.
 */
class CesiumCartographicPolygon : public Resource {
	GDCLASS(CesiumCartographicPolygon, Resource)

public:
	void set_vertices_degrees(const PackedVector2Array& vertices);
	PackedVector2Array get_vertices_degrees() const;
	bool set_vertices_degrees_exact(const PackedFloat64Array& components);
	PackedFloat64Array get_vertices_degrees_exact() const;
	bool set_vertices_from_ecef_exact(
		const PackedFloat64Array& ecefComponents,
		const Ref<CesiumEllipsoid>& ellipsoid
	);

	int32_t get_vertex_count() const;
	bool is_valid() const;
	PackedInt32Array get_triangulated_indices() const;
	PackedFloat64Array get_bounding_rectangle_degrees() const;
	bool rectangle_is_completely_inside_degrees(
		const PackedFloat64Array& westSouthEastNorth
	) const;
	bool rectangle_is_completely_outside_degrees(
		const PackedFloat64Array& westSouthEastNorth
	) const;

	CesiumGeospatial::CartographicPolygon create_native_polygon() const;

protected:
	static void _bind_methods();

private:
	void set_vertices_radians(std::vector<glm::dvec2>&& vertices);

	std::vector<glm::dvec2> m_verticesRadians;
};

#endif // CESIUM_CARTOGRAPHIC_POLYGON_H
