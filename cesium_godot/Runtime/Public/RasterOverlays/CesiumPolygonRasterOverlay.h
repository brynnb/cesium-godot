// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumPolygonRasterOverlay.h
// - Source/CesiumRuntime/Private/CesiumPolygonRasterOverlay.cpp
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_POLYGON_RASTER_OVERLAY_H
#define CESIUM_POLYGON_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

#include <memory>

namespace Cesium3DTilesSelection {
class ITileExcluder;
}

/**
 * Rasterizes cartographic polygons into a monochrome mask and optionally
 * excludes Native tiles that are wholly selected by that mask.
 */
class CesiumPolygonRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumPolygonRasterOverlay, CesiumRasterOverlay)

public:
	CesiumPolygonRasterOverlay();
	~CesiumPolygonRasterOverlay() override;

	void set_polygons(const Array& polygons);
	Array get_polygons() const;
	void set_invert_selection(bool invert);
	bool get_invert_selection() const;
	void set_exclude_selected_tiles(bool exclude);
	bool get_exclude_selected_tiles() const;
	int32_t get_valid_polygon_count() const;

	void _on_polygon_changed();

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;
	void on_added_to_tileset(
		Cesium3DTileset* tileset,
		CesiumRasterOverlays::RasterOverlay* overlay
	) override;
	void on_removing_from_tileset(
		Cesium3DTileset* tileset,
		CesiumRasterOverlays::RasterOverlay* overlay
	) override;

private:
	void disconnect_polygon_changes();
	void connect_polygon_changes();

	Array m_polygons;
	bool m_invertSelection = false;
	bool m_excludeSelectedTiles = true;
	std::shared_ptr<Cesium3DTilesSelection::ITileExcluder> m_polygonExcluder;
};

#endif // CESIUM_POLYGON_RASTER_OVERLAY_H
