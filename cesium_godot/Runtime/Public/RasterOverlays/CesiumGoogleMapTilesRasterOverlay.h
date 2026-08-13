// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumGoogleMapTilesRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GOOGLE_MAP_TILES_RASTER_OVERLAY_H
#define CESIUM_GOOGLE_MAP_TILES_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

#if defined(CESIUM_GD_MODULE)
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/variant/packed_string_array.hpp"
#endif

/** Raster imagery loaded directly from the Google Map Tiles API. */
class CesiumGoogleMapTilesRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumGoogleMapTilesRasterOverlay, CesiumRasterOverlay)

public:
	enum MapType {
		Satellite = 0,
		Roadmap = 1,
		Terrain = 2,
	};
	enum ImageFormat {
		Automatic = 0,
		Png = 1,
		Jpeg = 2,
	};
	enum Scale {
		Scale1x = 0,
		Scale2x = 1,
		Scale4x = 2,
	};
	enum LayerType {
		LayerRoadmap = 1,
		LayerStreetView = 2,
		LayerTraffic = 4,
	};

	void set_api_key(const String& value);
	const String& get_api_key() const;
	void set_map_type(int32_t value);
	int32_t get_map_type() const;
	void set_language(const String& value);
	const String& get_language() const;
	void set_region(const String& value);
	const String& get_region() const;
	void set_image_format(int32_t value);
	int32_t get_image_format() const;
	void set_scale(int32_t value);
	int32_t get_scale() const;
	void set_high_dpi(bool value);
	bool get_high_dpi() const;
	void set_layer_types(int32_t value);
	int32_t get_layer_types() const;
	void set_styles(const PackedStringArray& value);
	PackedStringArray get_styles() const;
	void set_overlay(bool value);
	bool get_overlay() const;
	void set_api_base_url(const String& value);
	const String& get_api_base_url() const;
	PackedStringArray get_last_style_errors() const;

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	String m_apiKey;
	MapType m_mapType = Satellite;
	String m_language = "en-US";
	String m_region = "US";
	ImageFormat m_imageFormat = Automatic;
	Scale m_scale = Scale1x;
	bool m_highDpi = false;
	int32_t m_layerTypes = 0;
	PackedStringArray m_styles;
	bool m_overlay = false;
	String m_apiBaseUrl = "https://tile.googleapis.com/";
	PackedStringArray m_lastStyleErrors;
};

VARIANT_ENUM_CAST(CesiumGoogleMapTilesRasterOverlay::MapType);
VARIANT_ENUM_CAST(CesiumGoogleMapTilesRasterOverlay::ImageFormat);
VARIANT_ENUM_CAST(CesiumGoogleMapTilesRasterOverlay::Scale);
VARIANT_ENUM_CAST(CesiumGoogleMapTilesRasterOverlay::LayerType);

#endif // CESIUM_GOOGLE_MAP_TILES_RASTER_OVERLAY_H
