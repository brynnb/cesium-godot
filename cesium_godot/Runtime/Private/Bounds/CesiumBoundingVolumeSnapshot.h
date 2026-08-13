// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_BOUNDING_VOLUME_SNAPSHOT_H
#define CESIUM_BOUNDING_VOLUME_SNAPSHOT_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/CalcBounds.* and Public/CesiumTile.h.
 *
 * The snapshot owns the Native bounding-volume variant. It never retains a
 * Cesium Tile, Tileset, or Godot Object pointer, so Resources remain valid
 * after their source tile is evicted.
 */

#include <Cesium3DTilesSelection/BoundingVolume.h>

#include <memory>

struct CesiumBoundingVolumeSnapshot {
	explicit CesiumBoundingVolumeSnapshot(
		const Cesium3DTilesSelection::BoundingVolume& source
	) : volume(source) {}

	Cesium3DTilesSelection::BoundingVolume volume;
};

std::shared_ptr<CesiumBoundingVolumeSnapshot>
create_cesium_bounding_volume_snapshot(
	const Cesium3DTilesSelection::BoundingVolume& source
);

#endif // CESIUM_BOUNDING_VOLUME_SNAPSHOT_H
