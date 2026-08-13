// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Metadata/CesiumPrimitiveMetadataSnapshot.h"

#include <CesiumGltf/AccessorUtility.h>
#include <CesiumGltf/ExtensionMeshPrimitiveExtStructuralMetadata.h>
#include <CesiumGltf/ExtensionModelExtStructuralMetadata.h>

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace {
uint64_t json_memory_bytes(const CesiumUtility::JsonValue& value) {
	if (value.isString()) {
		return static_cast<uint64_t>(value.getString().size());
	}
	if (value.isArray()) {
		uint64_t result = 0;
		for (const CesiumUtility::JsonValue& child : value.getArray()) {
			result += json_memory_bytes(child);
		}
		return result;
	}
	if (value.isObject()) {
		uint64_t result = 0;
		for (const auto& [key, child] : value.getObject()) {
			result += static_cast<uint64_t>(key.size());
			result += json_memory_bytes(child);
		}
		return result;
	}
	return sizeof(CesiumUtility::JsonValue);
}

uint64_t property_memory_bytes(
	const CesiumMetadataPropertySnapshot& property
) {
	uint64_t result = static_cast<uint64_t>(
		property.id.size() + property.type.size() +
		property.componentType.size() + property.enumType.size()
	);
	for (const CesiumUtility::JsonValue& value : property.values) {
		result += json_memory_bytes(value);
	}
	for (const CesiumUtility::JsonValue& value : property.rawValues) {
		result += json_memory_bytes(value);
	}
	result += json_memory_bytes(property.offset);
	result += json_memory_bytes(property.scale);
	result += json_memory_bytes(property.minimum);
	result += json_memory_bytes(property.maximum);
	result += json_memory_bytes(property.noData);
	result += json_memory_bytes(property.defaultValue);
	return result;
}
}

std::shared_ptr<CesiumPrimitiveMetadataSnapshot>
create_cesium_primitive_metadata_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const std::shared_ptr<const CesiumModelMetadataSnapshot>& modelMetadata,
	const std::vector<glm::dvec3>& vertexPositions,
	const std::vector<int64_t>& triangleIndices
) {
	const auto* primitiveMetadata = primitive.getExtension<
		CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata
	>();
	if (primitiveMetadata == nullptr) {
		return nullptr;
	}
	auto result = std::make_shared<CesiumPrimitiveMetadataSnapshot>();
	result->propertyTextureIndices = primitiveMetadata->propertyTextures;
	result->propertyAttributeIndices = primitiveMetadata->propertyAttributes;
	result->memoryBytes += static_cast<uint64_t>(
		result->propertyTextureIndices.size() +
		result->propertyAttributeIndices.size()
	) * sizeof(int32_t);

	const auto* sourceModelMetadata = model.getExtension<
		CesiumGltf::ExtensionModelExtStructuralMetadata
	>();
	if (sourceModelMetadata != nullptr) {
		for (int32_t attributeIndex : result->propertyAttributeIndices) {
			if (
				attributeIndex < 0 ||
				attributeIndex >= static_cast<int32_t>(
					sourceModelMetadata->propertyAttributes.size()
				)
			) {
				continue;
			}
			auto attribute = create_cesium_property_attribute_snapshot(
				model,
				primitive,
				sourceModelMetadata->propertyAttributes[
					static_cast<size_t>(attributeIndex)
				]
			);
			if (attribute != nullptr) {
				result->memoryBytes += static_cast<uint64_t>(
					attribute->name.size() + attribute->className.size()
				);
				for (const auto& property : attribute->properties) {
					if (property != nullptr) {
						result->memoryBytes += property_memory_bytes(*property);
					}
				}
				result->propertyAttributes.emplace_back(std::move(attribute));
			}
		}
	}

	std::unordered_set<int64_t> requiredCoordinateSets;
	if (modelMetadata != nullptr) {
		for (int32_t textureIndex : result->propertyTextureIndices) {
			if (
				textureIndex < 0 ||
				textureIndex >= static_cast<int32_t>(
					modelMetadata->propertyTextures.size()
				)
			) {
				continue;
			}
			const auto& texture =
				modelMetadata->propertyTextures[
					static_cast<size_t>(textureIndex)
				];
			if (texture == nullptr) {
				continue;
			}
			for (const auto& property : texture->properties) {
				if (
					property != nullptr &&
					property->status ==
						CesiumPropertyTexturePropertyStatus::Valid &&
					property->textureCoordinateSetIndex >= 0
				) {
					requiredCoordinateSets.emplace(
						property->textureCoordinateSetIndex
					);
				}
			}
		}
	}
	if (requiredCoordinateSets.empty()) {
		return result;
	}

	const int64_t vertexCount = static_cast<int64_t>(vertexPositions.size());
	if (
		triangleIndices.size() % 3 == 0 &&
		std::all_of(
			triangleIndices.begin(),
			triangleIndices.end(),
			[vertexCount](int64_t index) {
				return index >= 0 && index < vertexCount;
			}
		)
	) {
		result->vertexPositions = vertexPositions;
		result->triangleIndices = triangleIndices;
		result->memoryBytes += static_cast<uint64_t>(
			result->vertexPositions.size()
		) * sizeof(glm::dvec3);
		result->memoryBytes += static_cast<uint64_t>(
			result->triangleIndices.size()
		) * sizeof(int64_t);
	}

	for (int64_t coordinateSet : requiredCoordinateSets) {
		if (
			coordinateSet < 0 ||
			coordinateSet > std::numeric_limits<int32_t>::max()
		) {
			continue;
		}
		const CesiumGltf::TexCoordAccessorType accessor =
			CesiumGltf::getTexCoordAccessorView(
				model,
				primitive,
				static_cast<int32_t>(coordinateSet)
			);
		std::vector<glm::dvec2> coordinates;
		coordinates.reserve(static_cast<size_t>(vertexCount));
		bool valid = true;
		for (int64_t index = 0; index < vertexCount; ++index) {
			const std::optional<glm::dvec2> coordinate = std::visit(
				CesiumGltf::TexCoordFromAccessor{index},
				accessor
			);
			if (!coordinate) {
				valid = false;
				break;
			}
			coordinates.emplace_back(*coordinate);
		}
		if (valid) {
			result->memoryBytes += static_cast<uint64_t>(coordinates.size()) *
				sizeof(glm::dvec2);
			result->textureCoordinates.emplace(
				coordinateSet,
				std::move(coordinates)
			);
		}
	}
	return result;
}
