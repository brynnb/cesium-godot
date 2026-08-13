// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_LOD_TRANSITION_CONTROLLER_H
#define CESIUM_LOD_TRANSITION_CONTROLLER_H

#include <cstdint>

class Cesium3DTile;

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/Cesium3DTileset.cpp::updateTileFades
 * - Source/CesiumRuntime/Private/CesiumGltfComponent.cpp::UpdateFade
 *
 * Applies Cesium Native's per-tile transition percentage through a lightweight
 * per-placement ShaderMaterial duplicate. Meshes, textures, and Shader
 * resources remain shared; isolating only the parameter container supports the
 * Godot 4.6 Compatibility renderer, where instance shader uniforms are not
 * available. Last upstream review: Cesium for Unreal v2.29.0.
 */
class CesiumLodTransitionController final {
public:
	struct MaterialCoverage {
		int32_t supportedPrimitives = 0;
		int32_t unsupportedPrimitives = 0;
		int32_t compatibleRenderNodes = 0;
	};

	static MaterialCoverage apply(
		Cesium3DTile* tile,
		float percentage,
		bool fadingOut
	);
};

#endif // CESIUM_LOD_TRANSITION_CONTROLLER_H
