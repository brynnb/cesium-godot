// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's
// CesiumDebugColorizeTilesRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_DEBUG_COLORIZE_TILES_RASTER_OVERLAY_H
#define CESIUM_DEBUG_COLORIZE_TILES_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

/**
 * Debug overlay that gives every streamed geometry tile a random translucent
 * color. This makes selection, refinement, and LOD boundaries visible without
 * changing tileset content.
 */
class CesiumDebugColorizeTilesRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumDebugColorizeTilesRasterOverlay, CesiumRasterOverlay)

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;
};

#endif // CESIUM_DEBUG_COLORIZE_TILES_RASTER_OVERLAY_H
