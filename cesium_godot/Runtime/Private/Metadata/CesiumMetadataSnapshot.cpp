// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

#include <CesiumGltf/ClassProperty.h>
#include <CesiumGltf/ExtensionModelExtStructuralMetadata.h>
#include <CesiumGltf/Enum.h>
#include <CesiumGltf/PropertyAttributeView.h>
#include <CesiumGltf/PropertyTableView.h>
#include <CesiumGltf/PropertyTextureView.h>
#include <CesiumGltf/PropertyTypeTraits.h>

#include <optional>
#include <algorithm>
#include <unordered_set>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {
template <typename T>
CesiumUtility::JsonValue metadata_value_to_json(const T& value) {
	using Value = std::decay_t<T>;
	if constexpr (CesiumGltf::IsMetadataString<Value>::value) {
		return std::string(value.data(), value.size());
	} else if constexpr (CesiumGltf::IsMetadataBoolean<Value>::value) {
		return static_cast<bool>(value);
	} else if constexpr (CesiumGltf::IsMetadataScalar<Value>::value) {
		return CesiumUtility::JsonValue(value);
	} else if constexpr (CesiumGltf::IsMetadataVecN<Value>::value) {
		CesiumUtility::JsonValue::Array result;
		result.reserve(Value::length());
		for (glm::length_t index = 0; index < Value::length(); ++index) {
			result.emplace_back(metadata_value_to_json(value[index]));
		}
		return result;
	} else if constexpr (CesiumGltf::IsMetadataMatN<Value>::value) {
		CesiumUtility::JsonValue::Array columns;
		columns.reserve(Value::length());
		for (glm::length_t column = 0; column < Value::length(); ++column) {
			columns.emplace_back(metadata_value_to_json(value[column]));
		}
		return columns;
	} else if constexpr (CesiumGltf::IsMetadataArray<Value>::value) {
		CesiumUtility::JsonValue::Array result;
		result.reserve(static_cast<size_t>(value.size()));
		for (int64_t index = 0; index < value.size(); ++index) {
			result.emplace_back(metadata_value_to_json(value[index]));
		}
		return result;
	} else {
		static_assert(!sizeof(Value), "Unsupported structural metadata type");
	}
}

template <typename T>
CesiumUtility::JsonValue optional_metadata_to_json(
	const std::optional<T>& value
) {
	return value
		? metadata_value_to_json(*value)
		: CesiumUtility::JsonValue(nullptr);
}

CesiumMetadataPropertyStatus convert_property_status(int32_t status) {
	using Status = CesiumGltf::PropertyTablePropertyViewStatus;
	if (status == Status::Valid) {
		return CesiumMetadataPropertyStatus::Valid;
	}
	if (status == Status::EmptyPropertyWithDefault) {
		return CesiumMetadataPropertyStatus::EmptyPropertyWithDefault;
	}
	if (
		status == Status::ErrorInvalidPropertyTable ||
		status == Status::ErrorNonexistentProperty ||
		status == Status::ErrorTypeMismatch ||
		status == Status::ErrorComponentTypeMismatch ||
		status == Status::ErrorArrayTypeMismatch ||
		status == Status::ErrorInvalidNormalization ||
		status == Status::ErrorNormalizationMismatch ||
		status == Status::ErrorInvalidOffset ||
		status == Status::ErrorInvalidScale ||
		status == Status::ErrorInvalidMax ||
		status == Status::ErrorInvalidMin ||
		status == Status::ErrorInvalidNoDataValue ||
		status == Status::ErrorInvalidDefaultValue
	) {
		return CesiumMetadataPropertyStatus::ErrorInvalidProperty;
	}
	return CesiumMetadataPropertyStatus::ErrorInvalidPropertyData;
}

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

uint64_t metadata_property_memory_bytes(
	const CesiumMetadataPropertySnapshot& property
) {
	uint64_t result = sizeof(CesiumMetadataPropertySnapshot) +
		static_cast<uint64_t>(
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

CesiumMetadataPropertyStatus convert_attribute_property_status(
	int32_t status
) {
	using Status = CesiumGltf::PropertyAttributePropertyViewStatus;
	if (status == Status::Valid) {
		return CesiumMetadataPropertyStatus::Valid;
	}
	if (status == Status::EmptyPropertyWithDefault) {
		return CesiumMetadataPropertyStatus::EmptyPropertyWithDefault;
	}
	return status <= Status::ErrorInvalidPropertyAttribute
		? CesiumMetadataPropertyStatus::ErrorInvalidProperty
		: CesiumMetadataPropertyStatus::ErrorInvalidPropertyData;
}

CesiumPropertyTexturePropertyStatus convert_texture_property_status(
	int32_t status
) {
	using Status = CesiumGltf::PropertyTexturePropertyViewStatus;
	if (status == Status::Valid) {
		return CesiumPropertyTexturePropertyStatus::Valid;
	}
	if (status == Status::EmptyPropertyWithDefault) {
		return CesiumPropertyTexturePropertyStatus::EmptyPropertyWithDefault;
	}
	if (status == Status::ErrorUnsupportedProperty) {
		return CesiumPropertyTexturePropertyStatus::ErrorUnsupportedProperty;
	}
	return status < Status::ErrorUnsupportedProperty
		? CesiumPropertyTexturePropertyStatus::ErrorInvalidProperty
		: CesiumPropertyTexturePropertyStatus::ErrorInvalidPropertyData;
}

template <typename View>
void copy_property_definition(
	CesiumMetadataPropertySnapshot& property,
	const CesiumGltf::ClassProperty* classProperty,
	const View& propertyView
) {
	if (classProperty != nullptr) {
		property.type = classProperty->type;
		property.componentType = classProperty->componentType.value_or("");
		property.enumType = classProperty->enumType.value_or("");
		property.isArray = classProperty->array;
		property.normalized = classProperty->normalized;
		property.fixedArrayCount = classProperty->count.value_or(0);
	}
	property.offset = optional_metadata_to_json(propertyView.offset());
	property.scale = optional_metadata_to_json(propertyView.scale());
	property.minimum = optional_metadata_to_json(propertyView.min());
	property.maximum = optional_metadata_to_json(propertyView.max());
	property.noData = optional_metadata_to_json(propertyView.noData());
	property.defaultValue =
		optional_metadata_to_json(propertyView.defaultValue());
}

template <typename View>
void copy_texture_property_definition(
	CesiumPropertyTexturePropertySnapshot& property,
	const CesiumGltf::ClassProperty* classProperty,
	const View& propertyView
) {
	if (classProperty != nullptr) {
		property.type = classProperty->type;
		property.componentType = classProperty->componentType.value_or("");
		property.enumType = classProperty->enumType.value_or("");
		property.isArray = classProperty->array;
		property.normalized = classProperty->normalized;
		property.fixedArrayCount = classProperty->count.value_or(0);
	}
	property.offset = optional_metadata_to_json(propertyView.offset());
	property.scale = optional_metadata_to_json(propertyView.scale());
	property.minimum = optional_metadata_to_json(propertyView.min());
	property.maximum = optional_metadata_to_json(propertyView.max());
	property.noData = optional_metadata_to_json(propertyView.noData());
	property.defaultValue =
		optional_metadata_to_json(propertyView.defaultValue());
}
} // namespace

std::shared_ptr<CesiumModelMetadataSnapshot>
create_cesium_metadata_snapshot(const CesiumGltf::Model& model) {
	auto result = std::make_shared<CesiumModelMetadataSnapshot>();
	const auto* metadata = model.getExtension<
		CesiumGltf::ExtensionModelExtStructuralMetadata
	>();
	if (metadata == nullptr) {
		return result;
	}
	if (metadata->schema != nullptr) {
		result->enums.reserve(metadata->schema->enums.size());
		for (const auto& [enumId, sourceEnum] : metadata->schema->enums) {
			auto enumSnapshot =
				std::make_shared<CesiumMetadataEnumSnapshot>();
			enumSnapshot->id = enumId;
			enumSnapshot->name = sourceEnum.name.value_or("");
			enumSnapshot->description = sourceEnum.description.value_or("");
			enumSnapshot->valueType = sourceEnum.valueType;
			enumSnapshot->values.reserve(sourceEnum.values.size());
			for (const CesiumGltf::EnumValue& sourceValue : sourceEnum.values) {
				CesiumMetadataEnumValueSnapshot value;
				value.name = sourceValue.name;
				value.description = sourceValue.description.value_or("");
				value.value = sourceValue.value;
				enumSnapshot->values.emplace_back(std::move(value));
			}
			result->enums.emplace_back(std::move(enumSnapshot));
		}
		std::sort(
			result->enums.begin(),
			result->enums.end(),
			[](const auto& left, const auto& right) {
				return left->id < right->id;
			}
		);
	}

	result->propertyTables.reserve(metadata->propertyTables.size());
	for (const CesiumGltf::PropertyTable& sourceTable : metadata->propertyTables) {
		CesiumPropertyTableSnapshot table;
		table.name = sourceTable.name.value_or("");
		table.className = sourceTable.classProperty;
		table.count = sourceTable.count;
		CesiumGltf::PropertyTableView tableView(model, sourceTable);
		table.valid =
			tableView.status() == CesiumGltf::PropertyTableViewStatus::Valid;
		if (table.valid) {
			table.properties.reserve(sourceTable.properties.size());
			tableView.forEachProperty([
				&table,
				&tableView
			](const std::string& propertyId, const auto& propertyView) {
				auto property =
					std::make_shared<CesiumMetadataPropertySnapshot>();
				property->id = propertyId;
				property->status = convert_property_status(propertyView.status());
				const CesiumGltf::ClassProperty* classProperty =
					tableView.getClassProperty(propertyId);
				if (classProperty != nullptr) {
					property->type = classProperty->type;
					property->componentType =
						classProperty->componentType.value_or("");
					property->enumType = classProperty->enumType.value_or("");
					property->isArray = classProperty->array;
					property->normalized = classProperty->normalized;
					property->fixedArrayCount =
						classProperty->count.value_or(0);
				}
				property->offset = optional_metadata_to_json(propertyView.offset());
				property->scale = optional_metadata_to_json(propertyView.scale());
				property->minimum = optional_metadata_to_json(propertyView.min());
				property->maximum = optional_metadata_to_json(propertyView.max());
				property->noData = optional_metadata_to_json(propertyView.noData());
				property->defaultValue =
					optional_metadata_to_json(propertyView.defaultValue());

				if (
					property->status == CesiumMetadataPropertyStatus::Valid ||
					property->status ==
						CesiumMetadataPropertyStatus::EmptyPropertyWithDefault
				) {
					const int64_t count = propertyView.size();
					property->size = count;
					if (
						property->status ==
							CesiumMetadataPropertyStatus::EmptyPropertyWithDefault
					) {
						table.properties.emplace_back(std::move(property));
						return;
					}
					property->values.reserve(static_cast<size_t>(count));
					property->rawValues.reserve(static_cast<size_t>(count));
					for (int64_t index = 0; index < count; ++index) {
						auto value = propertyView.get(index);
						property->values.emplace_back(
							value
								? metadata_value_to_json(*value)
								: CesiumUtility::JsonValue(nullptr)
						);
						property->rawValues.emplace_back(
							metadata_value_to_json(propertyView.getRaw(index))
						);
					}
				}
				table.properties.emplace_back(std::move(property));
			});
		}
		result->propertyTables.emplace_back(std::move(table));
	}

	// A minimal retained model gives Native's typed property-texture views a
	// stable sampler/schema owner without copying geometry buffers. ImageAsset
	// payloads are intrusive-pointer shared, so multiple properties using one
	// image retain and charge that decoded image exactly once.
	auto textureModel = std::make_shared<CesiumGltf::Model>();
	textureModel->samplers = model.samplers;
	textureModel->textures = model.textures;
	textureModel->images = model.images;
	auto& retainedMetadata = textureModel->addExtension<
		CesiumGltf::ExtensionModelExtStructuralMetadata
	>();
	retainedMetadata.schema = metadata->schema;
	retainedMetadata.propertyTextures = metadata->propertyTextures;
	std::unordered_set<const CesiumImage::ImageAsset*> countedTextureImages;
	result->propertyTextures.reserve(retainedMetadata.propertyTextures.size());
	for (
		const CesiumGltf::PropertyTexture& sourceTexture :
		retainedMetadata.propertyTextures
	) {
		auto texture = std::make_shared<CesiumPropertyTextureSnapshot>();
		texture->name = sourceTexture.name.value_or("");
		texture->className = sourceTexture.classProperty;
		CesiumGltf::PropertyTextureView textureView(
			*textureModel,
			sourceTexture
		);
		texture->status = textureView.status() ==
			CesiumGltf::PropertyTextureViewStatus::Valid
			? CesiumPropertyTextureStatus::Valid
			: CesiumPropertyTextureStatus::ErrorInvalidPropertyTextureClass;
		if (texture->status == CesiumPropertyTextureStatus::Valid) {
			CesiumGltf::TextureViewOptions options;
			options.applyKhrTextureTransformExtension = true;
			textureView.forEachProperty([
				&texture,
				&textureView,
				&result,
				&countedTextureImages,
				textureModel
			](const std::string& propertyId, const auto& propertyView) {
				auto property = std::make_shared<
					CesiumPropertyTexturePropertySnapshot
				>();
				property->id = propertyId;
				property->status = convert_texture_property_status(
					propertyView.status()
				);
				copy_texture_property_definition(
					*property,
					textureView.getClassProperty(propertyId),
					propertyView
				);
				if (
					property->status ==
						CesiumPropertyTexturePropertyStatus::Valid
				) {
					property->textureCoordinateSetIndex =
						propertyView.getTexCoordSetIndex();
					property->channels = propertyView.getChannels();
					const CesiumGltf::Sampler* sampler =
						propertyView.getSampler();
					if (sampler != nullptr) {
						property->wrapS = sampler->wrapS;
						property->wrapT = sampler->wrapT;
					}
					const CesiumImage::ImageAsset* image =
						propertyView.getImage();
					if (image != nullptr) {
						result->cpuImageAssets.emplace(image);
					}
					if (
						image != nullptr &&
						countedTextureImages.emplace(image).second
					) {
						result->textureBytes += static_cast<uint64_t>(
							std::max<int64_t>(0, image->getSizeBytes())
						);
					}
					const auto transform = propertyView.getTextureTransform();
					if (transform) {
						property->hasTextureTransform = true;
						property->textureOffsetX = transform->offset().x;
						property->textureOffsetY = transform->offset().y;
						property->textureScaleX = transform->scale().x;
						property->textureScaleY = transform->scale().y;
						property->textureRotation = transform->rotation();
						property->textureTransformCoordinateSetIndex =
							transform->getTexCoordSetIndex().value_or(-1);
					}
				}

				if (
					property->status ==
						CesiumPropertyTexturePropertyStatus::Valid ||
					property->status ==
						CesiumPropertyTexturePropertyStatus::
							EmptyPropertyWithDefault
				) {
					property->sampleValue = [propertyView, textureModel](
						double u,
						double v
					) -> CesiumUtility::JsonValue {
						const auto value = propertyView.get(u, v);
						return value
							? metadata_value_to_json(*value)
							: CesiumUtility::JsonValue(nullptr);
					};
				}
				if (
					property->status ==
					CesiumPropertyTexturePropertyStatus::Valid
				) {
					property->sampleRawValue = [propertyView, textureModel](
						double u,
						double v
					) -> CesiumUtility::JsonValue {
						return metadata_value_to_json(
							propertyView.getRaw(u, v)
						);
					};
				}
				texture->properties.emplace_back(std::move(property));
			}, options);
			std::sort(
				texture->properties.begin(),
				texture->properties.end(),
				[](const auto& left, const auto& right) {
					return left->id < right->id;
				}
			);
		}
		result->propertyTextures.emplace_back(std::move(texture));
	}
	// Each typed TextureView now owns an intrusive reference to exactly the
	// ImageAsset it samples and copies its texture transform. Keep the model's
	// sampler vector because TextureView intentionally retains a sampler pointer,
	// but release the temporary image/texture/schema containers so metadata does
	// not keep unrelated render textures or the complete source schema alive.
	std::vector<CesiumGltf::Image>().swap(textureModel->images);
	std::vector<CesiumGltf::Texture>().swap(textureModel->textures);
	textureModel->removeExtension<
		CesiumGltf::ExtensionModelExtStructuralMetadata
	>();
	result->memoryBytes += sizeof(CesiumGltf::Model);
	for (const CesiumGltf::Sampler& sampler : textureModel->samplers) {
		result->memoryBytes += static_cast<uint64_t>(
			std::max<int64_t>(0, sampler.getSizeBytes())
		);
	}
	for (const auto& metadataEnum : result->enums) {
		if (metadataEnum == nullptr) {
			continue;
		}
		result->memoryBytes += sizeof(CesiumMetadataEnumSnapshot) +
			static_cast<uint64_t>(
				metadataEnum->id.size() + metadataEnum->name.size() +
				metadataEnum->description.size() +
				metadataEnum->valueType.size()
			);
		for (const CesiumMetadataEnumValueSnapshot& value : metadataEnum->values) {
			result->memoryBytes += sizeof(CesiumMetadataEnumValueSnapshot) +
				static_cast<uint64_t>(
					value.name.size() + value.description.size()
				);
		}
	}
	for (const CesiumPropertyTableSnapshot& table : result->propertyTables) {
		result->memoryBytes += sizeof(CesiumPropertyTableSnapshot) +
			static_cast<uint64_t>(table.name.size() + table.className.size());
		for (const auto& property : table.properties) {
			if (property != nullptr) {
				result->memoryBytes += metadata_property_memory_bytes(*property);
			}
		}
	}
	for (const auto& texture : result->propertyTextures) {
		if (texture == nullptr) {
			continue;
		}
		result->memoryBytes += sizeof(CesiumPropertyTextureSnapshot) +
			static_cast<uint64_t>(
				texture->name.size() + texture->className.size()
			);
		for (const auto& property : texture->properties) {
			if (property == nullptr) {
				continue;
			}
			result->memoryBytes +=
				sizeof(CesiumPropertyTexturePropertySnapshot) +
				static_cast<uint64_t>(
					property->id.size() + property->type.size() +
					property->componentType.size() +
					property->enumType.size()
				) + static_cast<uint64_t>(property->channels.size()) *
					sizeof(int64_t);
			result->memoryBytes += json_memory_bytes(property->offset);
			result->memoryBytes += json_memory_bytes(property->scale);
			result->memoryBytes += json_memory_bytes(property->minimum);
			result->memoryBytes += json_memory_bytes(property->maximum);
			result->memoryBytes += json_memory_bytes(property->noData);
			result->memoryBytes += json_memory_bytes(property->defaultValue);
		}
	}
	return result;
}

std::shared_ptr<CesiumPropertyAttributeSnapshot>
create_cesium_property_attribute_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::PropertyAttribute& propertyAttribute
) {
	auto result = std::make_shared<CesiumPropertyAttributeSnapshot>();
	result->name = propertyAttribute.name.value_or("");
	result->className = propertyAttribute.classProperty;
	CesiumGltf::PropertyAttributeView attributeView(model, propertyAttribute);
	if (
		attributeView.status() !=
		CesiumGltf::PropertyAttributeViewStatus::Valid
	) {
		result->status =
			CesiumPropertyAttributeStatus::ErrorInvalidPropertyAttributeClass;
		return result;
	}
	result->status = CesiumPropertyAttributeStatus::Valid;
	attributeView.forEachProperty(primitive, [
		&result,
		&attributeView
	](const std::string& propertyId, const auto& propertyView) {
		auto property = std::make_shared<CesiumMetadataPropertySnapshot>();
		property->id = propertyId;
		property->status = convert_attribute_property_status(
			propertyView.status()
		);
		copy_property_definition(
			*property,
			attributeView.getClassProperty(propertyId),
			propertyView
		);
		if (
			property->status == CesiumMetadataPropertyStatus::Valid ||
			property->status ==
				CesiumMetadataPropertyStatus::EmptyPropertyWithDefault
		) {
			property->size = propertyView.size();
			if (result->elementCount == 0 && property->size > 0) {
				result->elementCount = property->size;
			}
			if (
				property->status == CesiumMetadataPropertyStatus::Valid
			) {
				property->values.reserve(
					static_cast<size_t>(property->size)
				);
				property->rawValues.reserve(
					static_cast<size_t>(property->size)
				);
				for (int64_t index = 0; index < property->size; ++index) {
					const auto value = propertyView.get(index);
					property->values.emplace_back(
						value
							? metadata_value_to_json(*value)
							: CesiumUtility::JsonValue(nullptr)
					);
					property->rawValues.emplace_back(
						metadata_value_to_json(propertyView.getRaw(index))
					);
				}
			}
		}
		result->properties.emplace_back(std::move(property));
	});
	std::sort(
		result->properties.begin(),
		result->properties.end(),
		[](const auto& left, const auto& right) {
			return left->id < right->id;
		}
	);
	return result;
}
