// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumTileExcluder.h
// - Source/CesiumRuntime/Private/CesiumTileExcluder.cpp
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_TILE_EXCLUDER_H
#define CESIUM_TILE_EXCLUDER_H

#if defined(CESIUM_GD_MODULE)
#include "core/variant/callable.h"
#include "scene/main/node.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/callable.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <memory>

namespace Cesium3DTilesSelection {
class ITileExcluder;
}

class Cesium3DTileset;
class CesiumTileExclusionContext;

/**
 * A component-style Node that lets an application reject complete Native tile
 * subtrees before their content is requested.
 *
 * Set `predicate` to a Callable receiving CesiumTileExclusionContext and
 * returning bool, or derive a GDScript and implement `_should_exclude(context)`.
 * The predicate runs synchronously during Cesium traversal and must be fast,
 * deterministic, and free of scene-tree mutation.
 */
class CesiumTileExcluder : public Node {
	GDCLASS(CesiumTileExcluder, Node)

public:
	CesiumTileExcluder();
	~CesiumTileExcluder() override;

	void set_enabled(bool enabled);
	bool get_enabled() const;
	void set_predicate(const Callable& predicate);
	Callable get_predicate() const;

	Error add_to_tileset(Cesium3DTileset* tileset = nullptr);
	void remove_from_tileset(Cesium3DTileset* tileset = nullptr);
	void refresh();
	bool is_added_to_tileset() const;

	int64_t get_frame_count() const;
	int64_t get_evaluation_count() const;
	int64_t get_excluded_count() const;
	int64_t get_invalid_result_count() const;
	int64_t get_last_frame_evaluation_count() const;
	int64_t get_last_frame_excluded_count() const;
	void reset_statistics();

	// Native adapter integration only.
	void begin_native_frame();
	bool evaluate_native_context(const Ref<CesiumTileExclusionContext>& context);

	void _ready() override;
	void _exit_tree() override;

protected:
	static void _bind_methods();

private:
	Cesium3DTileset* resolve_tileset() const;

	bool m_enabled = true;
	Callable m_predicate;
	ObjectID m_tileset;
	std::shared_ptr<Cesium3DTilesSelection::ITileExcluder> m_nativeAdapter;
	uint64_t m_frameCount = 0;
	uint64_t m_evaluationCount = 0;
	uint64_t m_excludedCount = 0;
	uint64_t m_invalidResultCount = 0;
	uint64_t m_currentFrameEvaluationCount = 0;
	uint64_t m_currentFrameExcludedCount = 0;
	uint64_t m_lastFrameEvaluationCount = 0;
	uint64_t m_lastFrameExcludedCount = 0;
};

#endif // CESIUM_TILE_EXCLUDER_H
