// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's
// CesiumDebugColorizeTilesRasterOverlay.cpp (Apache-2.0).

#include "Runtime/Public/RasterOverlays/CesiumDebugColorizeTilesRasterOverlay.h"

#include <CesiumRasterOverlays/DebugColorizeTilesRasterOverlay.h>

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumDebugColorizeTilesRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	return std::make_unique<
		CesiumRasterOverlays::DebugColorizeTilesRasterOverlay
	>(this->get_material_key().utf8().get_data(), options);
}

String CesiumDebugColorizeTilesRasterOverlay::provider_type() const {
	return "debug_colorize_tiles";
}

void CesiumDebugColorizeTilesRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	// These are fixed properties of Cesium Native's provider rather than
	// configurable Godot policy. Publishing them makes the diagnostic behavior
	// inspectable without exposing or retaining a Native provider pointer.
	result["color_assignment"] = "random_per_geometry_tile";
	result["image_width"] = 1;
	result["image_height"] = 1;
	result["opacity_byte"] = 127;
	result["opacity"] = 127.0 / 255.0;
	result["requests_additional_detail"] = false;
}

void CesiumDebugColorizeTilesRasterOverlay::_bind_methods() {}
