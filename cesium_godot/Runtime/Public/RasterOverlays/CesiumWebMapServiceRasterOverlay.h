// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumWebMapServiceRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_WEB_MAP_SERVICE_RASTER_OVERLAY_H
#define CESIUM_WEB_MAP_SERVICE_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

/** Raster imagery loaded directly from an OGC Web Map Service (WMS). */
class CesiumWebMapServiceRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumWebMapServiceRasterOverlay, CesiumRasterOverlay)

public:
	void set_base_url(const String& value);
	const String& get_base_url() const;
	void set_layers(const String& value);
	const String& get_layers() const;
	void set_version(const String& value);
	const String& get_version() const;
	void set_format(const String& value);
	const String& get_format() const;
	void set_tile_width(int32_t value);
	int32_t get_tile_width() const;
	void set_tile_height(int32_t value);
	int32_t get_tile_height() const;
	void set_minimum_level(int32_t value);
	int32_t get_minimum_level() const;
	void set_maximum_level(int32_t value);
	int32_t get_maximum_level() const;
	void set_request_headers(const Dictionary& value);
	Dictionary get_request_headers() const;

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	String m_baseUrl;
	String m_layers;
	String m_version = "1.3.0";
	String m_format = "image/png";
	int32_t m_tileWidth = 256;
	int32_t m_tileHeight = 256;
	int32_t m_minimumLevel = 0;
	int32_t m_maximumLevel = 14;
	Dictionary m_requestHeaders;
};

#endif // CESIUM_WEB_MAP_SERVICE_RASTER_OVERLAY_H
