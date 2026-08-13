// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Bounds/CesiumBoundingVolumeSnapshot.h"

std::shared_ptr<CesiumBoundingVolumeSnapshot>
create_cesium_bounding_volume_snapshot(
	const Cesium3DTilesSelection::BoundingVolume& source
) {
	return std::make_shared<CesiumBoundingVolumeSnapshot>(source);
}
