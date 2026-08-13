// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Diagnostics/CesiumTileDebugState.h"

int64_t CesiumTileDebugState::get_frame_number() const {
	return static_cast<int64_t>(this->m_frameNumber);
}

const String& CesiumTileDebugState::get_tile_id() const { return this->m_tileId; }
const String& CesiumTileDebugState::get_parent_tile_id() const { return this->m_parentTileId; }
const String& CesiumTileDebugState::get_hierarchy_path() const { return this->m_hierarchyPath; }
const String& CesiumTileDebugState::get_parent_hierarchy_path() const { return this->m_parentHierarchyPath; }
int32_t CesiumTileDebugState::get_depth() const { return this->m_depth; }
int32_t CesiumTileDebugState::get_selection_role() const {
	return static_cast<int32_t>(this->m_selectionRole);
}

String CesiumTileDebugState::get_selection_role_name() const {
	switch (this->m_selectionRole) {
	case SelectionRole::Selected: return "selected";
	case SelectionRole::Fading: return "fading";
	case SelectionRole::Ancestor: return "ancestor";
	case SelectionRole::Child: return "child";
	case SelectionRole::RoleUnknown:
	default: return "unknown";
	}
}

int32_t CesiumTileDebugState::get_load_state() const {
	return static_cast<int32_t>(this->m_loadState);
}

String CesiumTileDebugState::get_load_state_name() const {
	switch (this->m_loadState) {
	case LoadState::Unloading: return "unloading";
	case LoadState::FailedTemporarily: return "failed_temporarily";
	case LoadState::Unloaded: return "unloaded";
	case LoadState::ContentLoading: return "content_loading";
	case LoadState::ContentLoaded: return "content_loaded";
	case LoadState::Done: return "done";
	case LoadState::Failed: return "failed";
	default: return "unknown";
	}
}

int32_t CesiumTileDebugState::get_refine_mode() const {
	return static_cast<int32_t>(this->m_refineMode);
}

String CesiumTileDebugState::get_refine_mode_name() const {
	return this->m_refineMode == RefineMode::Add ? "add" : "replace";
}

double CesiumTileDebugState::get_geometric_error() const { return this->m_geometricError; }
int32_t CesiumTileDebugState::get_child_count() const { return this->m_childCount; }
int32_t CesiumTileDebugState::get_reference_count() const { return this->m_referenceCount; }
bool CesiumTileDebugState::get_needs_worker_thread_loading() const { return this->m_needsWorkerThreadLoading; }
bool CesiumTileDebugState::get_needs_main_thread_loading() const { return this->m_needsMainThreadLoading; }
bool CesiumTileDebugState::get_renderable() const { return this->m_renderable; }
bool CesiumTileDebugState::get_has_render_content() const { return this->m_hasRenderContent; }
bool CesiumTileDebugState::get_has_external_content() const { return this->m_hasExternalContent; }
bool CesiumTileDebugState::get_has_empty_content() const { return this->m_hasEmptyContent; }
Ref<CesiumBoundingVolume> CesiumTileDebugState::get_tile_bounds() const { return this->m_tileBounds; }
Ref<CesiumBoundingVolume> CesiumTileDebugState::get_content_bounds() const { return this->m_contentBounds; }
Ref<CesiumBoundingVolume> CesiumTileDebugState::get_viewer_request_bounds() const { return this->m_viewerRequestBounds; }
Transform3D CesiumTileDebugState::get_world_bounds_transform() const { return this->m_worldBoundsTransform; }

Dictionary CesiumTileDebugState::to_dictionary() const {
	Dictionary result;
	result["frame_number"] = this->get_frame_number();
	result["tile_id"] = this->m_tileId;
	result["parent_tile_id"] = this->m_parentTileId;
	result["hierarchy_path"] = this->m_hierarchyPath;
	result["parent_hierarchy_path"] = this->m_parentHierarchyPath;
	result["depth"] = this->m_depth;
	result["selection_role"] = this->get_selection_role();
	result["selection_role_name"] = this->get_selection_role_name();
	result["load_state"] = this->get_load_state();
	result["load_state_name"] = this->get_load_state_name();
	result["refine_mode"] = this->get_refine_mode();
	result["refine_mode_name"] = this->get_refine_mode_name();
	result["geometric_error"] = this->m_geometricError;
	result["child_count"] = this->m_childCount;
	result["reference_count"] = this->m_referenceCount;
	result["needs_worker_thread_loading"] = this->m_needsWorkerThreadLoading;
	result["needs_main_thread_loading"] = this->m_needsMainThreadLoading;
	result["renderable"] = this->m_renderable;
	result["has_render_content"] = this->m_hasRenderContent;
	result["has_external_content"] = this->m_hasExternalContent;
	result["has_empty_content"] = this->m_hasEmptyContent;
	result["tile_bounds"] = this->m_tileBounds;
	result["content_bounds"] = this->m_contentBounds;
	result["viewer_request_bounds"] = this->m_viewerRequestBounds;
	result["world_bounds_transform"] = this->m_worldBoundsTransform;
	return result;
}

void CesiumTileDebugState::initialize(
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
) {
	this->m_frameNumber = frameNumber;
	this->m_tileId = tileId;
	this->m_parentTileId = parentTileId;
	this->m_hierarchyPath = hierarchyPath;
	this->m_parentHierarchyPath = parentHierarchyPath;
	this->m_depth = depth;
	this->m_selectionRole = selectionRole;
	this->m_loadState = loadState;
	this->m_refineMode = refineMode;
	this->m_geometricError = geometricError;
	this->m_childCount = childCount;
	this->m_referenceCount = referenceCount;
	this->m_needsWorkerThreadLoading = needsWorkerThreadLoading;
	this->m_needsMainThreadLoading = needsMainThreadLoading;
	this->m_renderable = renderable;
	this->m_hasRenderContent = hasRenderContent;
	this->m_hasExternalContent = hasExternalContent;
	this->m_hasEmptyContent = hasEmptyContent;
	this->m_tileBounds = tileBounds;
	this->m_contentBounds = contentBounds;
	this->m_viewerRequestBounds = viewerRequestBounds;
	this->m_worldBoundsTransform = worldBoundsTransform;
}

void CesiumTileDebugState::_bind_methods() {
#define CESIUM_BIND_GETTER(name) ClassDB::bind_method(D_METHOD("get_" #name), &CesiumTileDebugState::get_##name)
	CESIUM_BIND_GETTER(frame_number);
	CESIUM_BIND_GETTER(tile_id);
	CESIUM_BIND_GETTER(parent_tile_id);
	CESIUM_BIND_GETTER(hierarchy_path);
	CESIUM_BIND_GETTER(parent_hierarchy_path);
	CESIUM_BIND_GETTER(depth);
	CESIUM_BIND_GETTER(selection_role);
	CESIUM_BIND_GETTER(selection_role_name);
	CESIUM_BIND_GETTER(load_state);
	CESIUM_BIND_GETTER(load_state_name);
	CESIUM_BIND_GETTER(refine_mode);
	CESIUM_BIND_GETTER(refine_mode_name);
	CESIUM_BIND_GETTER(geometric_error);
	CESIUM_BIND_GETTER(child_count);
	CESIUM_BIND_GETTER(reference_count);
	CESIUM_BIND_GETTER(needs_worker_thread_loading);
	CESIUM_BIND_GETTER(needs_main_thread_loading);
	CESIUM_BIND_GETTER(renderable);
	CESIUM_BIND_GETTER(has_render_content);
	CESIUM_BIND_GETTER(has_external_content);
	CESIUM_BIND_GETTER(has_empty_content);
	CESIUM_BIND_GETTER(tile_bounds);
	CESIUM_BIND_GETTER(content_bounds);
	CESIUM_BIND_GETTER(viewer_request_bounds);
	CESIUM_BIND_GETTER(world_bounds_transform);
#undef CESIUM_BIND_GETTER
	ClassDB::bind_method(D_METHOD("to_dictionary"), &CesiumTileDebugState::to_dictionary);

	ClassDB::bind_integer_constant(get_class_static(), "SelectionRole", "ROLE_UNKNOWN", SelectionRole::RoleUnknown);
	ClassDB::bind_integer_constant(get_class_static(), "SelectionRole", "ROLE_SELECTED", SelectionRole::Selected);
	ClassDB::bind_integer_constant(get_class_static(), "SelectionRole", "ROLE_FADING", SelectionRole::Fading);
	ClassDB::bind_integer_constant(get_class_static(), "SelectionRole", "ROLE_ANCESTOR", SelectionRole::Ancestor);
	ClassDB::bind_integer_constant(get_class_static(), "SelectionRole", "ROLE_CHILD", SelectionRole::Child);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_UNLOADING", LoadState::Unloading);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_FAILED_TEMPORARILY", LoadState::FailedTemporarily);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_UNLOADED", LoadState::Unloaded);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_CONTENT_LOADING", LoadState::ContentLoading);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_CONTENT_LOADED", LoadState::ContentLoaded);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_DONE", LoadState::Done);
	ClassDB::bind_integer_constant(get_class_static(), "LoadState", "LOAD_FAILED", LoadState::Failed);
	ClassDB::bind_integer_constant(get_class_static(), "RefineMode", "REFINE_ADD", RefineMode::Add);
	ClassDB::bind_integer_constant(get_class_static(), "RefineMode", "REFINE_REPLACE", RefineMode::Replace);

#define CESIUM_READ_ONLY_PROPERTY(type, name) \
	ADD_PROPERTY(PropertyInfo(type, #name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_" #name)
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, frame_number);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, tile_id);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, parent_tile_id);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, hierarchy_path);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, parent_hierarchy_path);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, depth);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, selection_role);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, selection_role_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, load_state);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, load_state_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, refine_mode);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, refine_mode_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::FLOAT, geometric_error);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, child_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, reference_count);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, needs_worker_thread_loading);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, needs_main_thread_loading);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, renderable);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, has_render_content);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, has_external_content);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, has_empty_content);
	CESIUM_READ_ONLY_PROPERTY(Variant::OBJECT, tile_bounds);
	CESIUM_READ_ONLY_PROPERTY(Variant::OBJECT, content_bounds);
	CESIUM_READ_ONLY_PROPERTY(Variant::OBJECT, viewer_request_bounds);
	CESIUM_READ_ONLY_PROPERTY(Variant::TRANSFORM3D, world_bounds_transform);
#undef CESIUM_READ_ONLY_PROPERTY
}
