// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_METADATA_SNAPSHOT_H
#define CESIUM_METADATA_SNAPSHOT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumModelMetadata.h
 * Source/CesiumRuntime/Public/CesiumPropertyTable.h
 * Source/CesiumRuntime/Public/CesiumPropertyTableProperty.h
 * Source/CesiumRuntime/Public/CesiumPropertyAttribute.h
 * Source/CesiumRuntime/Public/CesiumPropertyTexture.h
 *
 * These snapshots own CPU-only copies. They may be prepared on a load worker
 * and retained after the source glTF and streamed tile are destroyed.
 */

#include <CesiumGltf/Model.h>
#include <CesiumGltf/PropertyAttribute.h>
#include <CesiumImage/ImageAsset.h>
#include <CesiumUtility/JsonValue.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

enum class CesiumMetadataPropertyStatus : int32_t {
	Valid = 0,
	EmptyPropertyWithDefault = 1,
	ErrorInvalidProperty = 2,
	ErrorInvalidPropertyData = 3,
};

struct CesiumMetadataPropertySnapshot {
	std::string id;
	CesiumMetadataPropertyStatus status =
		CesiumMetadataPropertyStatus::ErrorInvalidProperty;
	std::string type;
	std::string componentType;
	std::string enumType;
	bool isArray = false;
	bool normalized = false;
	int64_t fixedArrayCount = 0;
	int64_t size = 0;
	std::vector<CesiumUtility::JsonValue> values;
	std::vector<CesiumUtility::JsonValue> rawValues;
	CesiumUtility::JsonValue offset;
	CesiumUtility::JsonValue scale;
	CesiumUtility::JsonValue minimum;
	CesiumUtility::JsonValue maximum;
	CesiumUtility::JsonValue noData;
	CesiumUtility::JsonValue defaultValue;
};

struct CesiumPropertyTableSnapshot {
	bool valid = false;
	std::string name;
	std::string className;
	int64_t count = 0;
	std::vector<std::shared_ptr<CesiumMetadataPropertySnapshot>> properties;
};

enum class CesiumPropertyAttributeStatus : int32_t {
	Valid = 0,
	ErrorInvalidPropertyAttribute = 1,
	ErrorInvalidPropertyAttributeClass = 2,
};

struct CesiumPropertyAttributeSnapshot {
	CesiumPropertyAttributeStatus status =
		CesiumPropertyAttributeStatus::ErrorInvalidPropertyAttribute;
	std::string name;
	std::string className;
	int64_t elementCount = 0;
	std::vector<std::shared_ptr<CesiumMetadataPropertySnapshot>> properties;
};

enum class CesiumPropertyTextureStatus : int32_t {
	Valid = 0,
	ErrorInvalidPropertyTexture = 1,
	ErrorInvalidPropertyTextureClass = 2,
};

enum class CesiumPropertyTexturePropertyStatus : int32_t {
	Valid = 0,
	EmptyPropertyWithDefault = 1,
	ErrorInvalidProperty = 2,
	ErrorInvalidPropertyData = 3,
	ErrorUnsupportedProperty = 4,
};

struct CesiumPropertyTexturePropertySnapshot {
	std::string id;
	CesiumPropertyTexturePropertyStatus status =
		CesiumPropertyTexturePropertyStatus::ErrorInvalidProperty;
	std::string type;
	std::string componentType;
	std::string enumType;
	bool isArray = false;
	bool normalized = false;
	int64_t fixedArrayCount = 0;
	int64_t textureCoordinateSetIndex = -1;
	std::vector<int64_t> channels;
	int32_t wrapS = 10497;
	int32_t wrapT = 10497;
	bool hasTextureTransform = false;
	double textureOffsetX = 0.0;
	double textureOffsetY = 0.0;
	double textureScaleX = 1.0;
	double textureScaleY = 1.0;
	double textureRotation = 0.0;
	int64_t textureTransformCoordinateSetIndex = -1;
	CesiumUtility::JsonValue offset;
	CesiumUtility::JsonValue scale;
	CesiumUtility::JsonValue minimum;
	CesiumUtility::JsonValue maximum;
	CesiumUtility::JsonValue noData;
	CesiumUtility::JsonValue defaultValue;
	std::function<CesiumUtility::JsonValue(double, double)> sampleValue;
	std::function<CesiumUtility::JsonValue(double, double)> sampleRawValue;
};

struct CesiumPropertyTextureSnapshot {
	CesiumPropertyTextureStatus status =
		CesiumPropertyTextureStatus::ErrorInvalidPropertyTexture;
	std::string name;
	std::string className;
	std::vector<std::shared_ptr<CesiumPropertyTexturePropertySnapshot>>
		properties;
};

struct CesiumMetadataEnumValueSnapshot {
	std::string name;
	std::string description;
	int64_t value = 0;
};

struct CesiumMetadataEnumSnapshot {
	std::string id;
	std::string name;
	std::string description;
	std::string valueType;
	std::vector<CesiumMetadataEnumValueSnapshot> values;
};

struct CesiumModelMetadataSnapshot {
	std::vector<std::shared_ptr<CesiumMetadataEnumSnapshot>> enums;
	std::vector<CesiumPropertyTableSnapshot> propertyTables;
	std::vector<std::shared_ptr<CesiumPropertyTextureSnapshot>> propertyTextures;
	uint64_t memoryBytes = 0;
	uint64_t textureBytes = 0;
	// Property-texture sampling happens after renderer realization, so these
	// images must retain their CPU pixels. Ordinary material images can release
	// theirs as soon as Godot has uploaded the ImageTexture.
	std::unordered_set<const CesiumImage::ImageAsset*> cpuImageAssets;
};

std::shared_ptr<CesiumModelMetadataSnapshot>
create_cesium_metadata_snapshot(const CesiumGltf::Model& model);

std::shared_ptr<CesiumPropertyAttributeSnapshot>
create_cesium_property_attribute_snapshot(
	const CesiumGltf::Model& model,
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::PropertyAttribute& propertyAttribute
);

#endif // CESIUM_METADATA_SNAPSHOT_H
