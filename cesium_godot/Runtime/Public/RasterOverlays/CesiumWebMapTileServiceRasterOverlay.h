// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumWebMapTileServiceRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_WEB_MAP_TILE_SERVICE_RASTER_OVERLAY_H
#define CESIUM_WEB_MAP_TILE_SERVICE_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumRasterOverlayProjection.h"

#if defined(CESIUM_GD_MODULE)
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/variant/packed_string_array.hpp>
#endif

/** Raster imagery loaded directly from an OGC Web Map Tile Service (WMTS). */
class CesiumWebMapTileServiceRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumWebMapTileServiceRasterOverlay, CesiumRasterOverlay)

public:
	void set_base_url(const String& value);
	const String& get_base_url() const;
	void set_layer(const String& value);
	const String& get_layer() const;
	void set_style(const String& value);
	const String& get_style() const;
	void set_format(const String& value);
	const String& get_format() const;
	void set_tile_matrix_set_id(const String& value);
	const String& get_tile_matrix_set_id() const;
	void set_tile_matrix_set_label_prefix(const String& value);
	const String& get_tile_matrix_set_label_prefix() const;
	void set_specify_tile_matrix_set_labels(bool value);
	bool get_specify_tile_matrix_set_labels() const;
	void set_tile_matrix_set_labels(const PackedStringArray& value);
	PackedStringArray get_tile_matrix_set_labels() const;
	void set_projection(int32_t value);
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
	void set_specify_zoom_levels(bool value);
	bool get_specify_zoom_levels() const;
	void set_minimum_level(int32_t value);
	int32_t get_minimum_level() const;
	void set_maximum_level(int32_t value);
	int32_t get_maximum_level() const;
	void set_tile_width(int32_t value);
	int32_t get_tile_width() const;
	void set_tile_height(int32_t value);
	int32_t get_tile_height() const;
	void set_subdomains(const PackedStringArray& value);
	PackedStringArray get_subdomains() const;
	void set_dimensions(const Dictionary& value);
	Dictionary get_dimensions() const;
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
	String m_layer;
	String m_style;
	String m_format = "image/jpeg";
	String m_tileMatrixSetId;
	String m_tileMatrixSetLabelPrefix;
	bool m_specifyTileMatrixSetLabels = false;
	PackedStringArray m_tileMatrixSetLabels;
	CesiumRasterOverlayProjection m_projection = CesiumRasterOverlayProjection::WebMercator;
	bool m_specifyTilingScheme = false;
	int32_t m_rootTilesX = 1;
	int32_t m_rootTilesY = 1;
	real_t m_rectangleWest = -180.0;
	real_t m_rectangleSouth = -90.0;
	real_t m_rectangleEast = 180.0;
	real_t m_rectangleNorth = 90.0;
	bool m_specifyZoomLevels = false;
	int32_t m_minimumLevel = 0;
	int32_t m_maximumLevel = 25;
	int32_t m_tileWidth = 256;
	int32_t m_tileHeight = 256;
	PackedStringArray m_subdomains;
	Dictionary m_dimensions;
	Dictionary m_requestHeaders;
};

#endif // CESIUM_WEB_MAP_TILE_SERVICE_RASTER_OVERLAY_H
