// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Materials/CesiumLodTransitionController.h"

#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/geometry_instance3d.hpp"
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/shader.hpp"
#include "godot_cpp/classes/shader_material.hpp"
using namespace godot;
#endif

#include <algorithm>

namespace {
constexpr const char* LOD_PERCENTAGE_PARAMETER =
	"cesium_lod_transition_percentage";
constexpr const char* LOD_FADING_OUT_PARAMETER = "cesium_lod_fading_out";
constexpr const char* LOD_SUPPORT_META = "cesium_lod_transition_supported";
constexpr const char* LOD_OWNER_META = "cesium_lod_transition_owner_tile";

bool material_supports_lod_transition(const Ref<Material>& material) {
	if (material.is_null()) {
		return false;
	}
	if (material->has_meta(LOD_SUPPORT_META)) {
		return static_cast<bool>(material->get_meta(LOD_SUPPORT_META, false));
	}

	ShaderMaterial* shaderMaterial = Object::cast_to<ShaderMaterial>(
		material.ptr()
	);
	Ref<Shader> shader = shaderMaterial != nullptr
		? shaderMaterial->get_shader()
		: Ref<Shader>();
	const String code = shader.is_valid() ? shader->get_code() : String();
	const bool supported =
		code.contains("uniform float cesium_lod_transition_percentage") &&
		code.contains("uniform bool cesium_lod_fading_out");
	// Material selection/customization is complete before the first transition
	// update. Cache the capability on that final material so thousands of
	// visible primitives do not rescan shared shader text every frame.
	material->set_meta(LOD_SUPPORT_META, supported);
	return supported;
}
} // namespace

CesiumLodTransitionController::MaterialCoverage
CesiumLodTransitionController::apply(
	Cesium3DTile* tile,
	float percentage,
	bool fadingOut
) {
	MaterialCoverage coverage;
	if (tile == nullptr) {
		return coverage;
	}
	const float boundedPercentage = std::clamp(percentage, 0.0f, 1.0f);
	bool countedTileRenderNode = false;
	const int32_t primitiveCount = tile->get_loaded_tile_primitive_count();
	for (int32_t index = 0; index < primitiveCount; ++index) {
		Ref<CesiumLoadedTilePrimitive> primitive =
			tile->get_loaded_tile_primitive(index);
		if (primitive.is_null()) {
			continue;
		}
		Ref<Material> activeMaterial = primitive->get_active_material();
		GeometryInstance3D* renderNode = primitive->get_render_node();
		if (
			renderNode == nullptr ||
			!material_supports_lod_transition(activeMaterial)
		) {
			++coverage.unsupportedPrimitives;
			continue;
		}

		// Keep meshes, shaders, and textures shared, but isolate the uniform
		// container per tile placement. Godot 4.6 Compatibility does not support
		// instance shader uniforms, so a shallow Material duplicate is the portable
		// equivalent of Cesium for Unreal's per-component fade parameter.
		const int64_t ownerTileId = static_cast<int64_t>(tile->get_instance_id());
		const bool alreadyIsolated = static_cast<int64_t>(
			activeMaterial->get_meta(LOD_OWNER_META, int64_t(0))
		) == ownerTileId;
		Ref<Material> transitionMaterial = activeMaterial;
		if (!alreadyIsolated) {
			transitionMaterial = activeMaterial->duplicate(false);
			if (transitionMaterial.is_null()) {
				++coverage.unsupportedPrimitives;
				continue;
			}
			transitionMaterial->set_meta(LOD_SUPPORT_META, true);
			transitionMaterial->set_meta(LOD_OWNER_META, ownerTileId);
			if (renderNode == tile) {
				const int32_t meshSurfaceIndex =
					primitive->get_mesh_surface_index();
				if (meshSurfaceIndex < 0) {
					++coverage.unsupportedPrimitives;
					continue;
				}
				tile->set_surface_override_material(
					meshSurfaceIndex,
					transitionMaterial
				);
			} else {
				renderNode->set_material_override(transitionMaterial);
			}
		}

		ShaderMaterial* shaderMaterial = Object::cast_to<ShaderMaterial>(
			transitionMaterial.ptr()
		);
		if (shaderMaterial == nullptr) {
			++coverage.unsupportedPrimitives;
			continue;
		}
		shaderMaterial->set_shader_parameter(
			LOD_PERCENTAGE_PARAMETER,
			boundedPercentage
		);
		shaderMaterial->set_shader_parameter(
			LOD_FADING_OUT_PARAMETER,
			fadingOut
		);
		renderNode->set_meta(
			"cesium_lod_transition_percentage",
			boundedPercentage
		);
		renderNode->set_meta("cesium_lod_fading_out", fadingOut);
		++coverage.supportedPrimitives;
		if (renderNode != tile || !countedTileRenderNode) {
			++coverage.compatibleRenderNodes;
			countedTileRenderNode = countedTileRenderNode || renderNode == tile;
		}
	}
	return coverage;
}
