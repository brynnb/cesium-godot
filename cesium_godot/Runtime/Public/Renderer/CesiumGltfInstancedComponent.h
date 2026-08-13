// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_GLTF_INSTANCED_COMPONENT_H
#define CESIUM_GLTF_INSTANCED_COMPONENT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/CesiumGltfInstancedComponent.*.
 */

#if defined(CESIUM_GD_MODULE)
#include "scene/3d/multimesh_instance_3d.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/multi_mesh_instance3d.hpp"
#include "godot_cpp/classes/multi_mesh.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/aabb.hpp"
using namespace godot;
#endif

class CesiumPrimitiveFeatures;

/** Renders one glTF primitive through EXT_mesh_gpu_instancing. */
class CesiumGltfInstancedComponent : public MultiMeshInstance3D {
	GDCLASS(CesiumGltfInstancedComponent, MultiMeshInstance3D)

public:
	void initialize(
		int32_t rendererPrimitiveIndex,
		const Ref<MultiMesh>& multiMesh,
		const AABB& instanceBounds,
		const Transform3D& nodeTransform,
		const Ref<CesiumPrimitiveFeatures>& instanceFeatures
	);

	int32_t get_renderer_primitive_index() const;
	int32_t get_instance_count() const;
	Ref<CesiumPrimitiveFeatures> get_instance_features() const;

protected:
	static void _bind_methods();

private:
	int32_t m_rendererPrimitiveIndex = -1;
	Ref<CesiumPrimitiveFeatures> m_instanceFeatures;
};

#endif // CESIUM_GLTF_INSTANCED_COMPONENT_H
