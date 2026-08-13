// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterpart:
// - Source/CesiumRuntime/Public/CesiumPointCloudShading.h
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_POINT_CLOUD_SHADING_H
#define CESIUM_POINT_CLOUD_SHADING_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/math/vector3.h"
#include "core/variant/dictionary.h"
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <cstdint>

/**
 * Controls the screen-space realization of glTF POINTS primitives.
 *
 * This mirrors Cesium for Unreal's FCesiumPointCloudShading settings while
 * using Godot's native PRIMITIVE_POINTS / POINT_SIZE shader path. Changes are
 * live: a Cesium3DTileset updates already-realized points without reloading
 * tiles or rebuilding geometry.
 */
class CesiumPointCloudShading : public Resource {
	GDCLASS(CesiumPointCloudShading, Resource)

public:
	void set_attenuation(bool enabled);
	bool get_attenuation() const;

	void set_geometric_error_scale(real_t scale);
	real_t get_geometric_error_scale() const;

	void set_maximum_attenuation(real_t pixels);
	real_t get_maximum_attenuation() const;

	void set_base_resolution(real_t meters);
	real_t get_base_resolution() const;

	/** Resolves the same fallback hierarchy as Cesium for Unreal. */
	double resolve_geometric_error(
		double tileGeometricError,
		const Vector3& dimensions,
		int64_t pointCount
	) const;

	/** Returns all resolved per-draw inputs without mutating a material. */
	Dictionary resolve_parameters(
		bool usesAdditiveRefinement,
		double tileGeometricError,
		const Vector3& dimensions,
		int64_t pointCount,
		int64_t diameter,
		double maximumScreenSpaceError
	) const;

	/** Applies resolved values to a generated Cesium point material. */
	bool apply_to_material(
		const Ref<Material>& material,
		bool usesAdditiveRefinement,
		double tileGeometricError,
		const Vector3& dimensions,
		int64_t pointCount,
		int64_t diameter,
		double maximumScreenSpaceError
	) const;

protected:
	static void _bind_methods();

private:
	bool m_attenuation = false;
	real_t m_geometricErrorScale = 1.0;
	real_t m_maximumAttenuation = 0.0;
	real_t m_baseResolution = 0.0;
};

#endif // CESIUM_POINT_CLOUD_SHADING_H
