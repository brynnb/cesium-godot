// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PRIMITIVE_METADATA_SNAPSHOT_H
#define CESIUM_PRIMITIVE_METADATA_SNAPSHOT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/Private CesiumPrimitiveMetadata files.
 *
 * The snapshot owns property-attribute values and the positions, indices, and
 * UVs needed for exact property-texture hit queries. It never retains a glTF,
 * accessor, tile, or Godot Object pointer.
 */

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

#include <CesiumGltf/Model.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

struct CesiumPrimitiveMetadataSnapshot {
	std::vector<int32_t> propertyTextureIndices;
	std::vector<int32_t> propertyAttributeIndices;
	std::vector<std::shared_ptr<CesiumPropertyAttributeSnapshot>>
		propertyAttributes;
	std::vector<glm::dvec3> vertexPositions;
	std::vector<int64_t> triangleIndices;
	std::unordered_map<int64_t, std::vector<glm::dvec2>> textureCoordinates;
	uint64_t memoryBytes = 0;
};

std::shared_ptr<CesiumPrimitiveMetadataSnapshot>
create_cesium_primitive_metadata_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const std::shared_ptr<const CesiumModelMetadataSnapshot>& modelMetadata,
	const std::vector<glm::dvec3>& vertexPositions,
	const std::vector<int64_t>& triangleIndices
);

#endif // CESIUM_PRIMITIVE_METADATA_SNAPSHOT_H
