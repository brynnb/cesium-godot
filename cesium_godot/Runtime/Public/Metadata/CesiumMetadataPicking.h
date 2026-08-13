// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_METADATA_PICKING_H
#define CESIUM_METADATA_PICKING_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/Private CesiumMetadataPickingBlueprintLibrary.
 *
 * Godot's direct-space-state ray API returns a Dictionary, so this utility
 * resolves that native result rather than introducing an engine-specific hit
 * wrapper. The returned primitive Resource remains safe to retain after the
 * source tile unloads.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/dictionary.hpp"
using namespace godot;
#endif

class CesiumLoadedTilePrimitive;

class CesiumMetadataPicking : public Object {
	GDCLASS(CesiumMetadataPicking, Object)

public:
	static Dictionary resolve_hit(
		const Dictionary& hit,
		int32_t featureIdSetIndex = 0
	);
	static Dictionary resolve_instance(
		const Ref<CesiumLoadedTilePrimitive>& primitive,
		int64_t instanceIndex,
		int32_t featureIdSetIndex = 0
	);
	static int64_t get_feature_id_from_instance(
		const Ref<CesiumLoadedTilePrimitive>& primitive,
		int64_t instanceIndex,
		int32_t featureIdSetIndex = 0
	);
	static Dictionary get_metadata_values_from_instance(
		const Ref<CesiumLoadedTilePrimitive>& primitive,
		int64_t instanceIndex,
		int32_t featureIdSetIndex = 0
	);
	static int64_t get_feature_id_from_hit(
		const Dictionary& hit,
		int32_t featureIdSetIndex = 0
	);
	static Dictionary get_metadata_values_from_hit(
		const Dictionary& hit,
		int32_t featureIdSetIndex = 0
	);
	static Dictionary get_property_texture_values_from_hit(
		const Dictionary& hit,
		int32_t propertyTextureIndex
	);

protected:
	static void _bind_methods();
};

#endif // CESIUM_METADATA_PICKING_H
