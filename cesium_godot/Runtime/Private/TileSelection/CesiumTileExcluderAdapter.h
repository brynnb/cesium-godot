// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_TILE_EXCLUDER_ADAPTER_H
#define CESIUM_TILE_EXCLUDER_ADAPTER_H

#include <Cesium3DTilesSelection/ITileExcluder.h>

#include <memory>

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/variant/packed_float64_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/core/object_id.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
using namespace godot;
#endif

class CesiumTileExclusionContext;
class CesiumBoundingVolume;
struct CesiumBoundingVolumeSnapshot;

/** Lifetime-safe bridge from Cesium Native traversal to a Godot excluder. */
class CesiumTileExcluderAdapter final
	: public Cesium3DTilesSelection::ITileExcluder {
public:
	explicit CesiumTileExcluderAdapter(const ObjectID& excluder);
	void startNewFrame() noexcept override;
	bool shouldExclude(
		const Cesium3DTilesSelection::Tile& tile
	) const noexcept override;

private:
	ObjectID m_excluder;
	mutable Ref<CesiumTileExclusionContext> m_context;
	mutable Ref<CesiumBoundingVolume> m_bounds;
	mutable std::shared_ptr<CesiumBoundingVolumeSnapshot> m_boundsSnapshot;
	mutable PackedFloat64Array m_tileTransformComponents;
};

#endif // CESIUM_TILE_EXCLUDER_ADAPTER_H
