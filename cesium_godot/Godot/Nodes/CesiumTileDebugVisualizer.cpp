// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Godot/Nodes/CesiumTileDebugVisualizer.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Runtime/Public/Diagnostics/CesiumTileDebugState.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/immediate_mesh.h"
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/base_material3d.hpp"
#include "godot_cpp/classes/immediate_mesh.hpp"
#include "godot_cpp/classes/label3d.hpp"
#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#endif

#include <algorithm>
#include <array>
#include <unordered_map>

namespace {
Color color_for_state(const CesiumTileDebugState& state) {
	if (
		state.get_load_state() == CesiumTileDebugState::LoadState::Failed ||
		state.get_load_state() == CesiumTileDebugState::LoadState::FailedTemporarily
	) {
		return Color(1.0, 0.12, 0.12, 1.0);
	}
	if (state.get_needs_main_thread_loading()) {
		return Color(0.93, 0.2, 1.0, 1.0);
	}
	if (state.get_needs_worker_thread_loading()) {
		return Color(1.0, 0.45, 0.05, 1.0);
	}
	switch (state.get_selection_role()) {
	case CesiumTileDebugState::SelectionRole::Selected:
		return Color(0.1, 1.0, 0.2, 1.0);
	case CesiumTileDebugState::SelectionRole::Fading:
		return Color(1.0, 0.9, 0.05, 1.0);
	case CesiumTileDebugState::SelectionRole::Ancestor:
		return Color(0.05, 0.85, 1.0, 1.0);
	case CesiumTileDebugState::SelectionRole::Child:
		return Color(0.55, 0.55, 0.62, 1.0);
	case CesiumTileDebugState::SelectionRole::RoleUnknown:
	default:
		return Color(1.0, 1.0, 1.0, 1.0);
	}
}

void add_line(
	const Ref<ImmediateMesh>& mesh,
	const Vector3& from,
	const Vector3& to,
	const Color& color
) {
	mesh->surface_set_color(color);
	mesh->surface_add_vertex(from);
	mesh->surface_set_color(color);
	mesh->surface_add_vertex(to);
}

std::array<Vector3, 8> box_corners(const Transform3D& transform) {
	return {
		transform.xform(Vector3(-1.0, -1.0, -1.0)),
		transform.xform(Vector3( 1.0, -1.0, -1.0)),
		transform.xform(Vector3( 1.0,  1.0, -1.0)),
		transform.xform(Vector3(-1.0,  1.0, -1.0)),
		transform.xform(Vector3(-1.0, -1.0,  1.0)),
		transform.xform(Vector3( 1.0, -1.0,  1.0)),
		transform.xform(Vector3( 1.0,  1.0,  1.0)),
		transform.xform(Vector3(-1.0,  1.0,  1.0)),
	};
}
} // namespace

void CesiumTileDebugVisualizer::set_tileset(Cesium3DTileset* tileset) {
	this->m_tileset = tileset == nullptr
		? ObjectID()
		: ObjectID(tileset->get_instance_id());
	if (tileset != nullptr && this->m_visualizationEnabled) {
		tileset->set_debug_tile_state_capture_enabled(true);
	}
	this->m_drawnFrameNumber = 0;
}

Cesium3DTileset* CesiumTileDebugVisualizer::get_tileset() const {
	if (this->m_tileset.is_null()) {
		return nullptr;
	}
	return Object::cast_to<Cesium3DTileset>(
		ObjectDB::get_instance(this->m_tileset)
	);
}

void CesiumTileDebugVisualizer::set_visualization_enabled(bool enabled) {
	if (this->m_visualizationEnabled == enabled) {
		return;
	}
	this->m_visualizationEnabled = enabled;
	Cesium3DTileset* tileset = this->get_tileset();
	if (enabled && tileset != nullptr) {
		tileset->set_debug_tile_state_capture_enabled(true);
	} else if (!enabled) {
		this->clear_visuals();
	}
}

bool CesiumTileDebugVisualizer::get_visualization_enabled() const {
	return this->m_visualizationEnabled;
}

void CesiumTileDebugVisualizer::set_show_labels(bool showLabels) {
	this->m_showLabels = showLabels;
	this->m_drawnFrameNumber = 0;
}

bool CesiumTileDebugVisualizer::get_show_labels() const {
	return this->m_showLabels;
}

void CesiumTileDebugVisualizer::set_show_refinement_relationships(
	bool showRelationships
) {
	this->m_showRefinementRelationships = showRelationships;
	this->m_drawnFrameNumber = 0;
}

bool CesiumTileDebugVisualizer::get_show_refinement_relationships() const {
	return this->m_showRefinementRelationships;
}

void CesiumTileDebugVisualizer::set_maximum_labels(int32_t maximumLabels) {
	this->m_maximumLabels = std::clamp(maximumLabels, 0, 512);
	this->m_drawnFrameNumber = 0;
}

int32_t CesiumTileDebugVisualizer::get_maximum_labels() const {
	return this->m_maximumLabels;
}

int32_t CesiumTileDebugVisualizer::get_drawn_state_count() const {
	return this->m_drawnStateCount;
}

int64_t CesiumTileDebugVisualizer::get_drawn_frame_number() const {
	return static_cast<int64_t>(this->m_drawnFrameNumber);
}

void CesiumTileDebugVisualizer::_ready() {
	this->ensure_render_nodes();
	if (this->get_tileset() == nullptr) {
		this->set_tileset(Object::cast_to<Cesium3DTileset>(this->get_parent()));
	}
	this->set_process(true);
}

void CesiumTileDebugVisualizer::_process(double) {
	if (!this->m_visualizationEnabled) {
		return;
	}
	Cesium3DTileset* tileset = this->get_tileset();
	if (tileset == nullptr) {
		if (this->m_drawnFrameNumber != 0 || this->m_drawnStateCount != 0) {
			this->clear_visuals();
		}
		return;
	}
	tileset->set_debug_tile_state_capture_enabled(true);
	const uint64_t frameNumber = static_cast<uint64_t>(
		std::max<int64_t>(0, tileset->get_debug_tile_state_frame_number())
	);
	if (frameNumber == 0 || frameNumber == this->m_drawnFrameNumber) {
		return;
	}
	this->redraw();
}

void CesiumTileDebugVisualizer::_exit_tree() {
	this->clear_visuals();
}

void CesiumTileDebugVisualizer::ensure_render_nodes() {
	if (this->m_immediateMesh.is_null()) {
		this->m_immediateMesh.instantiate();
	}
	if (this->m_lineMaterial.is_null()) {
		Ref<StandardMaterial3D> material;
		material.instantiate();
		material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		material->set_flag(
			BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR,
			true
		);
		material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		this->m_lineMaterial = material;
	}
	MeshInstance3D* meshInstance = nullptr;
	if (!this->m_meshInstance.is_null()) {
		meshInstance = Object::cast_to<MeshInstance3D>(
			ObjectDB::get_instance(this->m_meshInstance)
		);
	}
	if (meshInstance == nullptr) {
		meshInstance = memnew(MeshInstance3D);
		meshInstance->set_name("TileDebugLines");
		meshInstance->set_mesh(this->m_immediateMesh);
		this->add_child(meshInstance, false);
		this->m_meshInstance = ObjectID(meshInstance->get_instance_id());
	}
}

Label3D* CesiumTileDebugVisualizer::acquire_label(int32_t index) {
	while (static_cast<int32_t>(this->m_labels.size()) <= index) {
		Label3D* label = memnew(Label3D);
		label->set_name("TileDebugLabel");
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		label->set_draw_flag(Label3D::FLAG_FIXED_SIZE, true);
		label->set_font_size(14);
		label->set_outline_size(4);
		label->set_pixel_size(0.0025);
		this->add_child(label, false);
		this->m_labels.emplace_back(label->get_instance_id());
	}
	Label3D* result = Object::cast_to<Label3D>(
		ObjectDB::get_instance(this->m_labels[index])
	);
	if (result != nullptr) {
		result->show();
	}
	return result;
}

void CesiumTileDebugVisualizer::hide_unused_labels(int32_t usedCount) {
	for (
		int32_t index = usedCount;
		index < static_cast<int32_t>(this->m_labels.size());
		++index
	) {
		Label3D* label = Object::cast_to<Label3D>(
			ObjectDB::get_instance(this->m_labels[index])
		);
		if (label != nullptr) {
			label->hide();
		}
	}
}

void CesiumTileDebugVisualizer::redraw() {
	this->ensure_render_nodes();
	Cesium3DTileset* tileset = this->get_tileset();
	if (tileset == nullptr || this->m_immediateMesh.is_null()) {
		this->clear_visuals();
		return;
	}

	const Array states = tileset->get_debug_tile_states();
	this->m_immediateMesh->clear_surfaces();
	this->m_drawnStateCount = 0;
	this->m_drawnFrameNumber = static_cast<uint64_t>(
		std::max<int64_t>(0, tileset->get_debug_tile_state_frame_number())
	);
	if (states.is_empty()) {
		this->hide_unused_labels(0);
		return;
	}

	const Transform3D worldToLocal = this->get_global_transform().affine_inverse();
	std::unordered_map<std::string, Vector3> centers;
	centers.reserve(states.size());
	this->m_immediateMesh->surface_begin(
		Mesh::PRIMITIVE_LINES,
		this->m_lineMaterial
	);
	static constexpr int32_t edgePairs[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	int32_t labelCount = 0;
	for (int32_t index = 0; index < states.size(); ++index) {
		Ref<CesiumTileDebugState> state = states[index];
		if (state.is_null() || state->get_tile_bounds().is_null()) {
			continue;
		}
		const Color color = color_for_state(*state.ptr());
		const Transform3D localBounds =
			worldToLocal * state->get_world_bounds_transform();
		const std::array<Vector3, 8> corners = box_corners(localBounds);
		for (const auto& edge : edgePairs) {
			add_line(
				this->m_immediateMesh,
				corners[edge[0]],
				corners[edge[1]],
				color
			);
		}
		const Vector3 center = localBounds.origin;
		centers.emplace(
			std::string(state->get_hierarchy_path().utf8().get_data()),
			center
		);
		++this->m_drawnStateCount;

		if (this->m_showLabels && labelCount < this->m_maximumLabels) {
			Label3D* label = this->acquire_label(labelCount++);
			if (label != nullptr) {
				label->set_position(center);
				label->set_modulate(color);
				label->set_text(
					(state->get_tile_id().is_empty()
						? state->get_hierarchy_path()
						: state->get_tile_id()) +
					String("  L") + String::num_int64(state->get_depth()) +
					String("  ") + state->get_selection_role_name() + String(" / ") +
					state->get_load_state_name() + String("\nGE ") +
					String::num(state->get_geometric_error(), 2) + String("  ") +
					state->get_refine_mode_name()
				);
			}
		}
	}

	if (this->m_showRefinementRelationships) {
		for (int32_t index = 0; index < states.size(); ++index) {
			Ref<CesiumTileDebugState> state = states[index];
			if (
				state.is_null() ||
				state->get_parent_hierarchy_path().is_empty()
			) {
				continue;
			}
			const auto child = centers.find(std::string(
				state->get_hierarchy_path().utf8().get_data()
			));
			const auto parent = centers.find(std::string(
				state->get_parent_hierarchy_path().utf8().get_data()
			));
			if (child != centers.end() && parent != centers.end()) {
				add_line(
					this->m_immediateMesh,
					child->second,
					parent->second,
					Color(0.8, 0.8, 0.8, 0.45)
				);
			}
		}
	}
	this->m_immediateMesh->surface_end();
	this->hide_unused_labels(labelCount);
}

void CesiumTileDebugVisualizer::clear_visuals() {
	if (this->m_immediateMesh.is_valid()) {
		this->m_immediateMesh->clear_surfaces();
	}
	this->hide_unused_labels(0);
	this->m_drawnStateCount = 0;
	this->m_drawnFrameNumber = 0;
}

void CesiumTileDebugVisualizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tileset", "tileset"), &CesiumTileDebugVisualizer::set_tileset);
	ClassDB::bind_method(D_METHOD("get_tileset"), &CesiumTileDebugVisualizer::get_tileset);
	ClassDB::bind_method(D_METHOD("set_visualization_enabled", "enabled"), &CesiumTileDebugVisualizer::set_visualization_enabled);
	ClassDB::bind_method(D_METHOD("get_visualization_enabled"), &CesiumTileDebugVisualizer::get_visualization_enabled);
	ClassDB::bind_method(D_METHOD("set_show_labels", "show_labels"), &CesiumTileDebugVisualizer::set_show_labels);
	ClassDB::bind_method(D_METHOD("get_show_labels"), &CesiumTileDebugVisualizer::get_show_labels);
	ClassDB::bind_method(D_METHOD("set_show_refinement_relationships", "show_relationships"), &CesiumTileDebugVisualizer::set_show_refinement_relationships);
	ClassDB::bind_method(D_METHOD("get_show_refinement_relationships"), &CesiumTileDebugVisualizer::get_show_refinement_relationships);
	ClassDB::bind_method(D_METHOD("set_maximum_labels", "maximum_labels"), &CesiumTileDebugVisualizer::set_maximum_labels);
	ClassDB::bind_method(D_METHOD("get_maximum_labels"), &CesiumTileDebugVisualizer::get_maximum_labels);
	ClassDB::bind_method(D_METHOD("get_drawn_state_count"), &CesiumTileDebugVisualizer::get_drawn_state_count);
	ClassDB::bind_method(D_METHOD("get_drawn_frame_number"), &CesiumTileDebugVisualizer::get_drawn_frame_number);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tileset", PROPERTY_HINT_NODE_TYPE, "Cesium3DTileset"), "set_tileset", "get_tileset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visualization_enabled"), "set_visualization_enabled", "get_visualization_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_labels"), "set_show_labels", "get_show_labels");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_refinement_relationships"), "set_show_refinement_relationships", "get_show_refinement_relationships");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_labels", PROPERTY_HINT_RANGE, "0,512,1"), "set_maximum_labels", "get_maximum_labels");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drawn_state_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_drawn_state_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "drawn_frame_number", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_drawn_frame_number");
}
