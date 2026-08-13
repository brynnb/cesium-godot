// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_TILE_DEBUG_STATE_H
#define CESIUM_TILE_DEBUG_STATE_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/Cesium3DTileset.cpp
 * - Source/CesiumRuntime/Private/DebugTileStateDatabase.cpp
 *
 * An immutable, lifetime-safe description of one tile at one selection frame.
 * It deliberately owns only copied values and RefCounted bounding-volume
 * snapshots. It never retains a Cesium Native Tile or Tileset pointer.
 * Last upstream review: Cesium for Unreal v2.29.0.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/math/transform_3d.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/transform3d.hpp"
using namespace godot;
#endif

#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"

#include <cstdint>

class CesiumTileDebugState : public RefCounted {
	GDCLASS(CesiumTileDebugState, RefCounted)

public:
	enum SelectionRole {
		RoleUnknown = 0,
		Selected = 1,
		Fading = 2,
		Ancestor = 3,
		Child = 4,
	};

	enum LoadState {
		Unloading = -2,
		FailedTemporarily = -1,
		Unloaded = 0,
		ContentLoading = 1,
		ContentLoaded = 2,
		Done = 3,
		Failed = 4,
	};

	enum RefineMode {
		Add = 0,
		Replace = 1,
	};

	int64_t get_frame_number() const;
	const String& get_tile_id() const;
	const String& get_parent_tile_id() const;
	const String& get_hierarchy_path() const;
	const String& get_parent_hierarchy_path() const;
	int32_t get_depth() const;
	int32_t get_selection_role() const;
	String get_selection_role_name() const;
	int32_t get_load_state() const;
	String get_load_state_name() const;
	int32_t get_refine_mode() const;
	String get_refine_mode_name() const;
	double get_geometric_error() const;
	int32_t get_child_count() const;
	int32_t get_reference_count() const;
	bool get_needs_worker_thread_loading() const;
	bool get_needs_main_thread_loading() const;
	bool get_renderable() const;
	bool get_has_render_content() const;
	bool get_has_external_content() const;
	bool get_has_empty_content() const;
	Ref<CesiumBoundingVolume> get_tile_bounds() const;
	Ref<CesiumBoundingVolume> get_content_bounds() const;
	Ref<CesiumBoundingVolume> get_viewer_request_bounds() const;
	Transform3D get_world_bounds_transform() const;
	Dictionary to_dictionary() const;

	// Cesium3DTileset integration only.
	void initialize(
		uint64_t frameNumber,
		const String& tileId,
		const String& parentTileId,
		const String& hierarchyPath,
		const String& parentHierarchyPath,
		int32_t depth,
		SelectionRole selectionRole,
		LoadState loadState,
		RefineMode refineMode,
		double geometricError,
		int32_t childCount,
		int32_t referenceCount,
		bool needsWorkerThreadLoading,
		bool needsMainThreadLoading,
		bool renderable,
		bool hasRenderContent,
		bool hasExternalContent,
		bool hasEmptyContent,
		const Ref<CesiumBoundingVolume>& tileBounds,
		const Ref<CesiumBoundingVolume>& contentBounds,
		const Ref<CesiumBoundingVolume>& viewerRequestBounds,
		const Transform3D& worldBoundsTransform
	);

protected:
	static void _bind_methods();

private:
	uint64_t m_frameNumber = 0;
	String m_tileId;
	String m_parentTileId;
	String m_hierarchyPath;
	String m_parentHierarchyPath;
	int32_t m_depth = 0;
	SelectionRole m_selectionRole = SelectionRole::RoleUnknown;
	LoadState m_loadState = LoadState::Unloaded;
	RefineMode m_refineMode = RefineMode::Replace;
	double m_geometricError = 0.0;
	int32_t m_childCount = 0;
	int32_t m_referenceCount = 0;
	bool m_needsWorkerThreadLoading = false;
	bool m_needsMainThreadLoading = false;
	bool m_renderable = false;
	bool m_hasRenderContent = false;
	bool m_hasExternalContent = false;
	bool m_hasEmptyContent = false;
	Ref<CesiumBoundingVolume> m_tileBounds;
	Ref<CesiumBoundingVolume> m_contentBounds;
	Ref<CesiumBoundingVolume> m_viewerRequestBounds;
	Transform3D m_worldBoundsTransform;
};

VARIANT_ENUM_CAST(CesiumTileDebugState::SelectionRole);
VARIANT_ENUM_CAST(CesiumTileDebugState::LoadState);
VARIANT_ENUM_CAST(CesiumTileDebugState::RefineMode);

#endif // CESIUM_TILE_DEBUG_STATE_H
