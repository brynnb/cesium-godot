// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PRIMITIVE_FEATURES_SNAPSHOT_H
#define CESIUM_PRIMITIVE_FEATURES_SNAPSHOT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/Private CesiumPrimitiveFeatures and
 * CesiumFeatureIdSet files.
 *
 * These snapshots own all feature IDs needed for per-face property-table
 * queries. They never retain AccessorView, TextureView, glTF, or tile pointers.
 */

#include <CesiumGltf/Model.h>
#include <CesiumGltf/KhrTextureTransform.h>
#include <CesiumGltf/Sampler.h>
#include <CesiumGltf/TextureView.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

enum class CesiumFeatureIdSetType : int32_t {
	None = 0,
	Attribute = 1,
	Texture = 2,
	Implicit = 3,
	Instance = 4,
	InstanceImplicit = 5,
};

enum class CesiumFeatureIdSetStatus : int32_t {
	Valid = 0,
	ErrorInvalidDefinition = 1,
	ErrorInvalidAccessor = 2,
	ErrorInvalidTexture = 3,
};

struct CesiumFeatureIdSetSnapshot {
	CesiumFeatureIdSetType type = CesiumFeatureIdSetType::None;
	CesiumFeatureIdSetStatus status =
		CesiumFeatureIdSetStatus::ErrorInvalidDefinition;
	int64_t featureCount = 0;
	int64_t nullFeatureId = -1;
	int32_t propertyTableIndex = -1;
	std::string label;
	int64_t attributeIndex = -1;
	int32_t textureIndex = -1;
	int64_t textureCoordinateSetIndex = -1;
	int64_t instanceCount = 0;
	std::vector<int64_t> vertexFeatureIds;
	std::vector<int64_t> instanceFeatureIds;
	std::vector<glm::dvec2> vertexTextureCoordinates;
	std::shared_ptr<CesiumGltf::Sampler> textureSampler;
	std::shared_ptr<CesiumGltf::TextureView> textureView;
	std::optional<CesiumGltf::KhrTextureTransform> textureTransform;
	std::vector<int64_t> textureChannels;
};

struct CesiumPrimitiveFeaturesSnapshot {
	int64_t vertexCount = 0;
	int64_t instanceCount = 0;
	uint64_t memoryBytes = 0;
	uint64_t textureBytes = 0;
	std::vector<glm::dvec3> vertexPositions;
	std::vector<int64_t> triangleIndices;
	std::vector<std::shared_ptr<CesiumFeatureIdSetSnapshot>> featureIdSets;
};

std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
create_cesium_primitive_features_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const std::vector<glm::dvec3>& vertexPositions,
	const std::vector<int64_t>& triangleIndices
);

std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
create_cesium_instance_features_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::Node& node,
	int64_t instanceCount
);

#endif // CESIUM_PRIMITIVE_FEATURES_SNAPSHOT_H
