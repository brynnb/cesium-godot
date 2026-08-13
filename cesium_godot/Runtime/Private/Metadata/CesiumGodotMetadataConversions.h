#ifndef CESIUM_GODOT_METADATA_CONVERSIONS_H
#define CESIUM_GODOT_METADATA_CONVERSIONS_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/UnrealMetadataConversions.h
 *
 * Cesium for Unreal converts metadata into Unreal reflection values. This
 * boundary performs the equivalent lossless conversion into Godot Variants.
 */

#include <CesiumUtility/JsonValue.h>

#if defined(CESIUM_GD_MODULE)
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/variant.hpp"
using namespace godot;
#endif

namespace CesiumGodotMetadataConversions {

Variant json_value_to_variant(const CesiumUtility::JsonValue& value);
Dictionary json_object_to_dictionary(
	const CesiumUtility::JsonValue::Object& object
);

} // namespace CesiumGodotMetadataConversions

#endif // CESIUM_GODOT_METADATA_CONVERSIONS_H
