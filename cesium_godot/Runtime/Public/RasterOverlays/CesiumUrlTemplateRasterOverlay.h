// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumUrlTemplateRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_URL_TEMPLATE_RASTER_OVERLAY_H
#define CESIUM_URL_TEMPLATE_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumRasterOverlayProjection.h"

/** Raster imagery loaded from a URL template such as /{z}/{x}/{y}.png. */
class CesiumUrlTemplateRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumUrlTemplateRasterOverlay, CesiumRasterOverlay)

public:
	void set_template_url(const String& url);
	const String& get_template_url() const;
	void set_projection(int32_t projection);
	int32_t get_projection() const;
	void set_specify_tiling_scheme(bool value);
	bool get_specify_tiling_scheme() const;
	void set_root_tiles_x(int32_t value);
	int32_t get_root_tiles_x() const;
	void set_root_tiles_y(int32_t value);
	int32_t get_root_tiles_y() const;
	void set_rectangle_west(real_t value);
	real_t get_rectangle_west() const;
	void set_rectangle_south(real_t value);
	real_t get_rectangle_south() const;
	void set_rectangle_east(real_t value);
	real_t get_rectangle_east() const;
	void set_rectangle_north(real_t value);
	real_t get_rectangle_north() const;
	void set_minimum_level(int32_t value);
	int32_t get_minimum_level() const;
	void set_maximum_level(int32_t value);
	int32_t get_maximum_level() const;
	void set_tile_width(int32_t value);
	int32_t get_tile_width() const;
	void set_tile_height(int32_t value);
	int32_t get_tile_height() const;
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
	String m_templateUrl;
	CesiumRasterOverlayProjection m_projection =
		CesiumRasterOverlayProjection::WebMercator;
	bool m_specifyTilingScheme = false;
	int32_t m_rootTilesX = 1;
	int32_t m_rootTilesY = 1;
	real_t m_rectangleWest = -180.0;
	real_t m_rectangleSouth = -90.0;
	real_t m_rectangleEast = 180.0;
	real_t m_rectangleNorth = 90.0;
	int32_t m_minimumLevel = 0;
	int32_t m_maximumLevel = 25;
	int32_t m_tileWidth = 256;
	int32_t m_tileHeight = 256;
	Dictionary m_requestHeaders;
};

#endif // CESIUM_URL_TEMPLATE_RASTER_OVERLAY_H
