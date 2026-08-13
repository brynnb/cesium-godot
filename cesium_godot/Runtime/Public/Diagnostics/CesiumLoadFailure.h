// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_LOAD_FAILURE_H
#define CESIUM_LOAD_FAILURE_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Public/Cesium3DTilesetLoadFailureDetails.h
 * - Source/CesiumRuntime/Public/CesiumRasterOverlayLoadFailureDetails.h
 *
 * This immutable RefCounted snapshot deliberately extends the two upstream
 * detail structs with tile, renderer, material, cache, and retry context. It
 * never owns the source Node; source is resolved weakly from its instance ID.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/dictionary.hpp"
using namespace godot;
#endif

#include <cstdint>

class CesiumLoadFailure : public RefCounted {
	GDCLASS(CesiumLoadFailure, RefCounted)

public:
	enum Category {
		Unknown = 0,
		Tileset = 1,
		TileContent = 2,
		RasterOverlay = 3,
		Network = 4,
		Decode = 5,
		Material = 6,
		Renderer = 7,
		Cache = 8,
	};

	enum Stage {
		StageUnknown = 0,
		IonEndpoint = 1,
		TilesetJson = 2,
		TileContentRequest = 3,
		RasterTileProvider = 4,
		RasterTileRequest = 5,
		NetworkRequest = 6,
		GltfDecode = 7,
		MaterialCreation = 8,
		RendererPreparation = 9,
		TextureUpload = 10,
		CacheOpen = 11,
		CacheMaintenance = 12,
	};

	Object* get_source() const;
	int64_t get_source_instance_id() const;
	int32_t get_category() const;
	String get_category_name() const;
	int32_t get_stage() const;
	String get_stage_name() const;
	const String& get_message() const;
	const String& get_url() const;
	const String& get_tile_id() const;
	const String& get_overlay_key() const;
	int32_t get_http_status_code() const;
	bool get_retryable() const;
	bool get_terminal() const;
	bool get_retry_scheduled() const;
	int32_t get_attempt() const;
	int32_t get_maximum_attempts() const;
	double get_retry_delay_seconds() const;
	Dictionary to_dictionary() const;

	void initialize(
		uint64_t sourceInstanceId,
		Category category,
		Stage stage,
		const String& message,
		const String& url,
		const String& tileId,
		const String& overlayKey,
		int32_t httpStatusCode,
		bool terminal,
		bool retryable,
		bool retryScheduled,
		int32_t attempt,
		int32_t maximumAttempts,
		double retryDelaySeconds
	);

protected:
	static void _bind_methods();

private:
	uint64_t m_sourceInstanceId = 0;
	Category m_category = Category::Unknown;
	Stage m_stage = Stage::StageUnknown;
	String m_message;
	String m_url;
	String m_tileId;
	String m_overlayKey;
	int32_t m_httpStatusCode = 0;
	bool m_terminal = true;
	bool m_retryable = false;
	bool m_retryScheduled = false;
	int32_t m_attempt = 0;
	int32_t m_maximumAttempts = 0;
	double m_retryDelaySeconds = 0.0;
};

VARIANT_ENUM_CAST(CesiumLoadFailure::Category);
VARIANT_ENUM_CAST(CesiumLoadFailure::Stage);

#endif // CESIUM_LOAD_FAILURE_H
