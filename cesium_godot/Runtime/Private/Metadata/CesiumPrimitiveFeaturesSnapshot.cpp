// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Metadata/CesiumPrimitiveFeaturesSnapshot.h"

#include <CesiumGltf/AccessorUtility.h>
#include <CesiumGltf/ExtensionExtInstanceFeatures.h>
#include <CesiumGltf/ExtensionExtMeshFeatures.h>
#include <CesiumGltf/FeatureIdTextureView.h>

#include <algorithm>
#include <limits>
#include <optional>

std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
create_cesium_primitive_features_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const std::vector<glm::dvec3>& vertexPositions,
	const std::vector<int64_t>& triangleIndices
) {
	auto result = std::make_shared<CesiumPrimitiveFeaturesSnapshot>();
	result->vertexCount = static_cast<int64_t>(vertexPositions.size());
	const auto* features = primitive.getExtension<
		CesiumGltf::ExtensionExtMeshFeatures
	>();
	if (features == nullptr) {
		return result;
	}
	if (triangleIndices.size() % 3 == 0) {
		bool indicesValid = true;
		for (int64_t index : triangleIndices) {
			if (index < 0 || index >= result->vertexCount) {
				indicesValid = false;
				break;
			}
		}
		if (indicesValid) {
			result->triangleIndices = triangleIndices;
		}
	}

	result->featureIdSets.reserve(features->featureIds.size());
	for (const CesiumGltf::FeatureId& source : features->featureIds) {
		auto set = std::make_shared<CesiumFeatureIdSetSnapshot>();
		set->featureCount = source.featureCount;
		set->nullFeatureId = source.nullFeatureId.value_or(-1);
		set->propertyTableIndex = source.propertyTable;
		set->label = source.label.value_or("");

		if (source.attribute) {
			set->type = CesiumFeatureIdSetType::Attribute;
			set->attributeIndex = *source.attribute;
			const CesiumGltf::FeatureIdAccessorType accessor =
				CesiumGltf::getFeatureIdAccessorView(
					model,
					primitive,
					static_cast<int32_t>(*source.attribute)
				);
			if (
				std::visit(CesiumGltf::StatusFromAccessor(), accessor) ==
				CesiumGltf::AccessorViewStatus::Valid
			) {
					const int64_t count = std::visit(
					CesiumGltf::CountFromAccessor(),
					accessor
				);
				set->vertexFeatureIds.reserve(static_cast<size_t>(count));
				for (int64_t index = 0; index < count; ++index) {
					set->vertexFeatureIds.emplace_back(std::visit(
						CesiumGltf::FeatureIdFromAccessor{index},
						accessor
					));
				}
				set->status = CesiumFeatureIdSetStatus::Valid;
			} else {
				set->status = CesiumFeatureIdSetStatus::ErrorInvalidAccessor;
			}
		} else if (source.texture) {
			set->type = CesiumFeatureIdSetType::Texture;
			set->textureIndex = static_cast<int32_t>(source.texture->index);
			CesiumGltf::TextureViewOptions options;
			options.applyKhrTextureTransformExtension = true;
			const CesiumGltf::FeatureIdTextureView textureView(
				model,
				*source.texture,
				options
			);
			set->textureCoordinateSetIndex = textureView.getTexCoordSetIndex();
			if (
					textureView.status() ==
					CesiumGltf::FeatureIdTextureViewStatus::Valid
				) {
					// Exact hit sampling needs local triangle positions. Do not retain
					// this second position copy for the overwhelmingly common primitive
					// that has no texture-backed feature IDs.
					if (result->vertexPositions.empty()) {
						result->vertexPositions = vertexPositions;
					}
				set->textureSampler = std::make_shared<CesiumGltf::Sampler>(
					*textureView.getSampler()
				);
				set->textureView = std::make_shared<CesiumGltf::TextureView>(
					*set->textureSampler,
					*textureView.getImage(),
					set->textureCoordinateSetIndex
				);
				set->textureTransform = textureView.getTextureTransform();
				set->textureChannels = textureView.getChannels();
				const CesiumGltf::TexCoordAccessorType coordinates =
					CesiumGltf::getTexCoordAccessorView(
						model,
						primitive,
						static_cast<int32_t>(
							set->textureCoordinateSetIndex
						)
					);
				set->vertexFeatureIds.reserve(
					static_cast<size_t>(result->vertexCount)
				);
				set->vertexTextureCoordinates.reserve(
					static_cast<size_t>(result->vertexCount)
				);
				bool valid = true;
				for (int64_t index = 0; index < result->vertexCount; ++index) {
					const std::optional<glm::dvec2> coordinate = std::visit(
						CesiumGltf::TexCoordFromAccessor{index},
						coordinates
					);
					if (!coordinate) {
						valid = false;
						break;
					}
					set->vertexTextureCoordinates.emplace_back(*coordinate);
					set->vertexFeatureIds.emplace_back(
						textureView.getFeatureID(
							(*coordinate)[0],
							(*coordinate)[1]
						)
					);
				}
				if (valid) {
					set->status = CesiumFeatureIdSetStatus::Valid;
				} else {
					set->vertexFeatureIds.clear();
					set->vertexTextureCoordinates.clear();
					set->textureSampler.reset();
					set->textureView.reset();
					set->textureTransform.reset();
					set->textureChannels.clear();
					set->status =
						CesiumFeatureIdSetStatus::ErrorInvalidAccessor;
				}
			} else {
				set->status = CesiumFeatureIdSetStatus::ErrorInvalidTexture;
			}
		} else if (source.featureCount > 0) {
			set->type = CesiumFeatureIdSetType::Implicit;
			set->status = CesiumFeatureIdSetStatus::Valid;
		}

		result->featureIdSets.emplace_back(std::move(set));
	}
	result->memoryBytes =
		static_cast<uint64_t>(result->vertexPositions.size()) *
			sizeof(glm::dvec3) +
		static_cast<uint64_t>(result->triangleIndices.size()) *
			sizeof(int64_t);
	for (const auto& set : result->featureIdSets) {
		if (!set) {
			continue;
		}
		result->memoryBytes +=
			static_cast<uint64_t>(set->vertexFeatureIds.size()) *
				sizeof(int64_t) +
			static_cast<uint64_t>(set->vertexTextureCoordinates.size()) *
				sizeof(glm::dvec2) +
			static_cast<uint64_t>(set->textureChannels.size()) *
				sizeof(int64_t) +
			static_cast<uint64_t>(set->label.size());
		if (set->textureView && set->textureView->getImage()) {
			result->textureBytes += static_cast<uint64_t>(
				std::max<int64_t>(0, set->textureView->getImage()->getSizeBytes())
			);
		}
	}
	return result;
}

std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
create_cesium_instance_features_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::Node& node,
	int64_t instanceCount
) {
	const auto* features = node.getExtension<
		CesiumGltf::ExtensionExtInstanceFeatures
	>();
	if (features == nullptr) {
		return nullptr;
	}
	auto result = std::make_shared<CesiumPrimitiveFeaturesSnapshot>();
	result->instanceCount = std::max<int64_t>(0, instanceCount);
	result->featureIdSets.reserve(features->featureIds.size());
	for (
		const CesiumGltf::ExtensionExtInstanceFeaturesFeatureId& source :
		features->featureIds
	) {
		auto set = std::make_shared<CesiumFeatureIdSetSnapshot>();
		set->instanceCount = result->instanceCount;
		set->featureCount = source.featureCount;
		set->nullFeatureId = source.nullFeatureId.value_or(-1);
		set->label = source.label.value_or("");
		result->memoryBytes += static_cast<uint64_t>(set->label.size());
		const int64_t propertyTableIndex = source.propertyTable.value_or(-1);
		const bool invalidCommonDefinition = source.featureCount <= 0 ||
			(source.nullFeatureId && *source.nullFeatureId < 0) ||
			(source.propertyTable && (
				propertyTableIndex < 0 ||
				propertyTableIndex > std::numeric_limits<int32_t>::max()
			));
		if (invalidCommonDefinition) {
			result->featureIdSets.emplace_back(std::move(set));
			continue;
		}
		set->propertyTableIndex = static_cast<int32_t>(propertyTableIndex);

		if (source.attribute) {
			set->type = CesiumFeatureIdSetType::Instance;
			set->attributeIndex = *source.attribute;
			if (
				*source.attribute < 0 ||
				*source.attribute > std::numeric_limits<int32_t>::max()
			) {
				result->featureIdSets.emplace_back(std::move(set));
				continue;
			}
			const CesiumGltf::FeatureIdAccessorType accessor =
				CesiumGltf::getFeatureIdAccessorView(
					model,
					node,
					static_cast<int32_t>(*source.attribute)
				);
			const int64_t count = std::visit(
				CesiumGltf::CountFromAccessor(),
				accessor
			);
			if (
				std::visit(CesiumGltf::StatusFromAccessor(), accessor) ==
					CesiumGltf::AccessorViewStatus::Valid &&
				count == result->instanceCount
			) {
				set->instanceFeatureIds.reserve(static_cast<size_t>(count));
				bool valuesValid = true;
				for (int64_t index = 0; index < count; ++index) {
					const int64_t featureId = std::visit(
						CesiumGltf::FeatureIdFromAccessor{index},
						accessor
					);
					if (
						featureId < 0 ||
						(
							featureId >= set->featureCount &&
							featureId != set->nullFeatureId
						)
					) {
						valuesValid = false;
						break;
					}
					set->instanceFeatureIds.emplace_back(featureId);
				}
				if (valuesValid) {
					set->status = CesiumFeatureIdSetStatus::Valid;
				} else {
					set->instanceFeatureIds.clear();
					set->status =
						CesiumFeatureIdSetStatus::ErrorInvalidAccessor;
				}
			} else {
				set->status = CesiumFeatureIdSetStatus::ErrorInvalidAccessor;
			}
		} else if (result->instanceCount > 0) {
			const bool nullIsOneImplicitInstance = set->nullFeatureId >= 0 &&
				set->nullFeatureId < result->instanceCount;
			const int64_t expectedFeatureCount = result->instanceCount -
				(nullIsOneImplicitInstance ? 1 : 0);
			if (source.featureCount == expectedFeatureCount) {
				set->type = CesiumFeatureIdSetType::InstanceImplicit;
				set->status = CesiumFeatureIdSetStatus::Valid;
			}
		}

		result->memoryBytes +=
			static_cast<uint64_t>(set->instanceFeatureIds.size()) *
				sizeof(int64_t);
		result->featureIdSets.emplace_back(std::move(set));
	}
	return result;
}
