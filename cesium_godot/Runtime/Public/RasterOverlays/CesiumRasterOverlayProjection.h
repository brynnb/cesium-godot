// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal raster-overlay projection enums.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_RASTER_OVERLAY_PROJECTION_H
#define CESIUM_RASTER_OVERLAY_PROJECTION_H

#include <cstdint>

/** Projection used by raster providers that expose an explicit choice. */
enum class CesiumRasterOverlayProjection : int32_t {
	WebMercator = 0,
	Geographic = 1
};

#endif // CESIUM_RASTER_OVERLAY_PROJECTION_H
