// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_FEATURE_STYLE_ENCODING_H
#define CESIUM_FEATURE_STYLE_ENCODING_H

#include "Runtime/Private/Metadata/CesiumPrimitiveFeaturesSnapshot.h"

#include <cstdint>
#include <memory>
#include <string>

/**
 * Worker-safe selection copied from CesiumMetadataStyle before a tileset
 * generation starts. It deliberately contains no Godot Object or Variant.
 */
struct CesiumFeatureStyleEncodingDescription {
	bool enabled = false;
	bool useInstanceFeatures = false;
	int32_t featureIdSetIndex = 0;
	std::string featureIdSetName;
};

enum class CesiumFeatureStyleSource : int32_t {
	None = 0,
	Vertex = 1,
	Texture = 2,
	Instance = 3,
};

/**
 * Per-primitive result of resolving a requested feature-ID set. Vertex and
 * instance payloads are stored by CesiumGDPreparedPrimitive; texture-backed
 * sets retain this lifetime-safe snapshot until their Godot texture is made.
 */
struct CesiumFeatureStyleEncoding {
	bool requested = false;
	bool valid = false;
	CesiumFeatureStyleSource source = CesiumFeatureStyleSource::None;
	bool usesInstanceFeatures = false;
	int32_t featureIdSetIndex = -1;
	int64_t featureCount = 0;
	int64_t nullFeatureId = -1;
	int32_t propertyTableIndex = -1;
	std::string name;
	std::string diagnostic;
	std::shared_ptr<const CesiumFeatureIdSetSnapshot> featureIdSet;
};

#endif // CESIUM_FEATURE_STYLE_ENCODING_H
