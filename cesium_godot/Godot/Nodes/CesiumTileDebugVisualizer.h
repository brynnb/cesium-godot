// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_TILE_DEBUG_VISUALIZER_H
#define CESIUM_TILE_DEBUG_VISUALIZER_H

/**
 * Godot-native companion to Cesium for Unreal's tile debug drawing. It renders
 * immutable CesiumTileDebugState snapshots and never accesses Native Tiles.
 * Last upstream review: Cesium for Unreal v2.29.0.
 */

#if defined(CESIUM_GD_MODULE)
#include "scene/3d/node_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/immediate_mesh.h"
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/immediate_mesh.hpp"
#include "godot_cpp/classes/label3d.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/core/object.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <vector>

class Cesium3DTileset;

class CesiumTileDebugVisualizer : public Node3D {
	GDCLASS(CesiumTileDebugVisualizer, Node3D)

public:
	void set_tileset(Cesium3DTileset* tileset);
	Cesium3DTileset* get_tileset() const;
	void set_visualization_enabled(bool enabled);
	bool get_visualization_enabled() const;
	void set_show_labels(bool showLabels);
	bool get_show_labels() const;
	void set_show_refinement_relationships(bool showRelationships);
	bool get_show_refinement_relationships() const;
	void set_maximum_labels(int32_t maximumLabels);
	int32_t get_maximum_labels() const;
	int32_t get_drawn_state_count() const;
	int64_t get_drawn_frame_number() const;

	void _ready() override;
	void _process(double delta) override;
	void _exit_tree() override;

protected:
	static void _bind_methods();

private:
	void ensure_render_nodes();
	void redraw();
	void clear_visuals();
	Label3D* acquire_label(int32_t index);
	void hide_unused_labels(int32_t usedCount);

	ObjectID m_tileset;
	ObjectID m_meshInstance;
	Ref<ImmediateMesh> m_immediateMesh;
	Ref<StandardMaterial3D> m_lineMaterial;
	std::vector<ObjectID> m_labels;
	bool m_visualizationEnabled = true;
	bool m_showLabels = true;
	bool m_showRefinementRelationships = true;
	int32_t m_maximumLabels = 64;
	int32_t m_drawnStateCount = 0;
	uint64_t m_drawnFrameNumber = 0;
};

#endif // CESIUM_TILE_DEBUG_VISUALIZER_H
