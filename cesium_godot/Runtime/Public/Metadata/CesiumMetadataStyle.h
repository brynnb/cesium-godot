// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_METADATA_STYLE_H
#define CESIUM_METADATA_STYLE_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/image.h"
#include "core/io/image_texture.h"
#include "core/object/object.h"
#include "core/io/resource.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/image_texture.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <vector>

class CesiumLoadedTilePrimitive;
class CesiumModelMetadata;
struct CesiumFeatureStyleEncodingDescription;

/**
 * A live feature style for EXT_mesh_features / EXT_instance_features.
 *
 * This is the Godot-native counterpart of Cesium for Unreal's
 * CesiumFeaturesMetadataComponent plus EncodedFeaturesMetadata material path.
 * The selected feature-ID set is encoded when a tileset generation loads. The
 * ordered rules are evaluated into a small lookup texture and may then change
 * without rebuilding geometry or re-streaming the tileset.
 */
class CesiumMetadataStyle : public Resource {
	GDCLASS(CesiumMetadataStyle, Resource)

public:
	enum FeatureSource {
		MeshFeatures = 0,
		InstanceFeatures = 1,
	};

	void set_enabled(bool enabled);
	bool get_enabled() const;

	void set_feature_source(int32_t source);
	int32_t get_feature_source() const;

	void set_feature_id_set_name(const String& name);
	String get_feature_id_set_name() const;

	void set_feature_id_set_index(int32_t index);
	int32_t get_feature_id_set_index() const;

	void set_rules(const Array& rules);
	Array get_rules() const;

	void set_default_show(bool show);
	bool get_default_show() const;

	void set_default_color(const Color& color);
	Color get_default_color() const;

	void set_default_color_mix(real_t mix);
	real_t get_default_color_mix() const;

	void set_maximum_features(int64_t count);
	int64_t get_maximum_features() const;
	void set_maximum_cache_bytes(int64_t bytes);
	int64_t get_maximum_cache_bytes() const;
	int64_t get_lookup_cache_count() const;
	int64_t get_lookup_cache_bytes() const;

	/** Returns the resolved rule output without creating GPU resources. */
	Dictionary evaluate_feature(
		int64_t featureId,
		const Dictionary& metadata
	) const;

	/** Applies or disables this style on an already-realized primitive. */
	bool apply_to_primitive(
		const Ref<CesiumLoadedTilePrimitive>& primitive
	);
	void clear_from_primitive(
		const Ref<CesiumLoadedTilePrimitive>& primitive
	) const;

	// Renderer integration. The returned value owns no Godot objects.
	CesiumFeatureStyleEncodingDescription get_encoding_description() const;
	uint64_t get_encoding_revision() const;
	void clear_lookup_cache();

protected:
	static void _bind_methods();

private:
	struct LookupTextures {
		// Keep only a weak identity for streamed model metadata. The style must
		// not extend a tile generation's lifetime merely to reuse a lookup.
		ObjectID metadata;
		int32_t propertyTableIndex = -1;
		int64_t featureCount = 0;
		Ref<ImageTexture> colors;
		Ref<ImageTexture> controls;
		int32_t dimension = 0;
		int64_t retainedBytes = 0;
		uint64_t lastUse = 0;
	};

	void changed(bool encodingChanged);
	const LookupTextures* get_or_create_lookup(
		const Ref<CesiumModelMetadata>& metadata,
		int32_t propertyTableIndex,
		int64_t featureCount
	);
	void prune_lookup_cache(int64_t bytesNeeded);

	bool m_enabled = false;
	FeatureSource m_featureSource = MeshFeatures;
	String m_featureIdSetName;
	int32_t m_featureIdSetIndex = 0;
	Array m_rules;
	bool m_defaultShow = true;
	Color m_defaultColor = Color(1.0, 1.0, 1.0, 1.0);
	real_t m_defaultColorMix = 0.0;
	int64_t m_maximumFeatures = 1024 * 1024;
	int64_t m_maximumCacheBytes = 64 * 1024 * 1024;
	uint64_t m_encodingRevision = 1;
	uint64_t m_lookupUseCounter = 0;
	int64_t m_lookupCacheBytes = 0;
	std::vector<LookupTextures> m_lookupCache;
};

VARIANT_ENUM_CAST(CesiumMetadataStyle::FeatureSource);

#endif // CESIUM_METADATA_STYLE_H
