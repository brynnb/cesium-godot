// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumTileMapServiceRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_TILE_MAP_SERVICE_RASTER_OVERLAY_H
#define CESIUM_TILE_MAP_SERVICE_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

/** Raster imagery loaded from a Tile Map Service (TMS). */
class CesiumTileMapServiceRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumTileMapServiceRasterOverlay, CesiumRasterOverlay)

public:
	void set_url(const String& url);
	const String& get_url() const;
	void set_specify_zoom_levels(bool value);
	bool get_specify_zoom_levels() const;
	void set_minimum_level(int32_t value);
	int32_t get_minimum_level() const;
	void set_maximum_level(int32_t value);
	int32_t get_maximum_level() const;
	void set_request_headers(const Dictionary& headers);
	Dictionary get_request_headers() const;

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	String m_url;
	bool m_specifyZoomLevels = false;
	int32_t m_minimumLevel = 0;
	int32_t m_maximumLevel = 10;
	Dictionary m_requestHeaders;
};

#endif // CESIUM_TILE_MAP_SERVICE_RASTER_OVERLAY_H
