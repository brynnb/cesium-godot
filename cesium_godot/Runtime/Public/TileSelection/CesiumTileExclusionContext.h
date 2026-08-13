// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_TILE_EXCLUSION_CONTEXT_H
#define CESIUM_TILE_EXCLUSION_CONTEXT_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/packed_float64_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
using namespace godot;
#endif

#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"

#include <cstdint>

/**
 * Read-only facts for the tile currently being evaluated by a
 * CesiumTileExcluder. One context is deliberately reused by the adapter to
 * avoid allocating a Godot object for every visited tile. Applications must
 * inspect it only during the predicate call or copy `to_dictionary()`.
 */
class CesiumTileExclusionContext : public RefCounted {
	GDCLASS(CesiumTileExclusionContext, RefCounted)

public:
	const String& get_tile_id() const;
	int32_t get_depth() const;
	int32_t get_child_count() const;
	double get_geometric_error() const;
	int32_t get_refine_mode() const;
	String get_refine_mode_name() const;
	Ref<CesiumBoundingVolume> get_tile_bounds() const;
	PackedFloat64Array get_tile_transform_components() const;
	Dictionary to_dictionary() const;

	// Native adapter integration only.
	void initialize(
		const String& tileId,
		int32_t depth,
		int32_t childCount,
		double geometricError,
		int32_t refineMode,
		const Ref<CesiumBoundingVolume>& tileBounds,
		const PackedFloat64Array& tileTransformComponents
	);

protected:
	static void _bind_methods();

private:
	String m_tileId;
	int32_t m_depth = 0;
	int32_t m_childCount = 0;
	double m_geometricError = 0.0;
	int32_t m_refineMode = 1;
	Ref<CesiumBoundingVolume> m_tileBounds;
	PackedFloat64Array m_tileTransformComponents;
};

#endif // CESIUM_TILE_EXCLUSION_CONTEXT_H
