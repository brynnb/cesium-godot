// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_GLTF_MODEL_RESOURCE_CACHE_H
#define CESIUM_GLTF_MODEL_RESOURCE_CACHE_H

/**
 * Godot renderer-resource counterpart of Cesium for Unreal v2.29.0's
 * CesiumGltfComponent / LoadGltfResult ownership boundary.
 *
 * Cesium Native records the complete resolved content request URL in
 * `Cesium3DTiles_TileUrl`. When multiple tiles in one tileset generation use
 * that exact content, their immutable Godot mesh, material, metadata, and
 * image resources may be shared while each tile keeps its own scene node,
 * transform, visibility, raster bindings, and material overrides.
 */

#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveMetadata.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/resources/mesh.h"
#include "scene/resources/multimesh.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/multi_mesh.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/array.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct CesiumGltfSharedPrimitiveResource {
	int32_t nodeIndex = -1;
	int32_t meshIndex = -1;
	int32_t primitiveIndex = -1;
	bool isGpuInstanced = false;
	bool isTranslucent = false;
	int32_t instanceCount = 0;
	int32_t realizedSurfaceIndex = -1;
	Ref<Material> material;
	Ref<ArrayMesh> primitiveMesh;
	Ref<MultiMesh> multiMesh;
	Ref<CesiumPrimitiveFeatures> instanceFeatures;
	Ref<CesiumPrimitiveMetadata> primitiveMetadata;
};

struct CesiumGltfSharedModelResource {
	~CesiumGltfSharedModelResource();

	std::string contentKey;
	Ref<ArrayMesh> mesh;
	Ref<CesiumModelMetadata> metadata;
	Array primitiveFeatures;
	std::vector<CesiumGltfSharedPrimitiveResource> primitives;
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>> imageResources;
	uint64_t geometryBytes = 0;
	uint64_t textureBytes = 0;
	std::shared_ptr<CesiumTilesetRuntimeStatistics> statistics;
};

class CesiumGltfModelResourceCache {
public:
	explicit CesiumGltfModelResourceCache(
		const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics
	);

	std::shared_ptr<CesiumGltfSharedModelResource> acquire(
		const std::string& contentKey
	);
	std::shared_ptr<CesiumGltfSharedModelResource> publish(
		const std::string& contentKey,
		const std::shared_ptr<CesiumGltfSharedModelResource>& resource
	);

private:
	void prune_expired();

	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_statistics;
	std::unordered_map<
		std::string,
		std::weak_ptr<CesiumGltfSharedModelResource>
	> m_resources;
};

#endif // CESIUM_GLTF_MODEL_RESOURCE_CACHE_H
