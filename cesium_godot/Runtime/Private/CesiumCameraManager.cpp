// Copyright 2020-2026 CesiumGS, Inc. and Contributors

#include "Runtime/Public/CesiumCameraManager.h"

#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#elif defined(CESIUM_GD_MODULE)
#include "scene/main/viewport.h"
#include "scene/resources/world_3d.h"
#endif

#include <unordered_set>

namespace {
Array copy_array(const Array& source) {
	Array result;
	for (int32_t index = 0; index < source.size(); ++index) {
		result.push_back(source[index]);
	}
	return result;
}

bool same_world(Node3D* context, Camera3D* camera) {
	if (context == nullptr || camera == nullptr || !context->is_inside_tree() ||
		!camera->is_inside_tree()) {
		return false;
	}
	const Ref<World3D> contextWorld = context->get_world_3d();
	const Ref<World3D> cameraWorld = camera->get_world_3d();
	return contextWorld.is_valid() && cameraWorld.is_valid() &&
		contextWorld.ptr() == cameraWorld.ptr();
}
} // namespace

void CesiumCameraManager::set_use_viewport_camera(bool useViewportCamera) {
	this->m_useViewportCamera = useViewportCamera;
}

bool CesiumCameraManager::get_use_viewport_camera() const {
	return this->m_useViewportCamera;
}

void CesiumCameraManager::set_additional_camera_paths(const Array& paths) {
	Array normalized;
	for (int32_t index = 0; index < paths.size(); ++index) {
		const Variant value = paths[index];
		if (value.get_type() == Variant::NODE_PATH) {
			normalized.push_back(static_cast<NodePath>(value));
		} else {
			normalized.push_back(NodePath());
		}
	}
	this->m_additionalCameraPaths = normalized;
	this->normalize_enabled_count();
}

Array CesiumCameraManager::get_additional_camera_paths() const {
	return copy_array(this->m_additionalCameraPaths);
}

void CesiumCameraManager::set_additional_camera_enabled(const Array& enabled) {
	Array normalized;
	for (int32_t index = 0; index < enabled.size(); ++index) {
		normalized.push_back(static_cast<bool>(enabled[index]));
	}
	this->m_additionalCameraEnabled = normalized;
	this->normalize_enabled_count();
}

Array CesiumCameraManager::get_additional_camera_enabled() const {
	return copy_array(this->m_additionalCameraEnabled);
}

int32_t CesiumCameraManager::add_camera(
	const NodePath& cameraPath,
	bool enabled
) {
	const int32_t result = this->m_additionalCameraPaths.size();
	this->m_additionalCameraPaths.push_back(cameraPath);
	this->m_additionalCameraEnabled.push_back(enabled);
	return result;
}

bool CesiumCameraManager::remove_camera_at(int32_t index) {
	if (index < 0 || index >= this->m_additionalCameraPaths.size()) {
		return false;
	}
	this->m_additionalCameraPaths.remove_at(index);
	this->m_additionalCameraEnabled.remove_at(index);
	return true;
}

bool CesiumCameraManager::move_camera(int32_t fromIndex, int32_t toIndex) {
	const int32_t count = this->m_additionalCameraPaths.size();
	if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count) {
		return false;
	}
	if (fromIndex == toIndex) {
		return true;
	}
	const Variant path = this->m_additionalCameraPaths[fromIndex];
	const Variant enabled = this->m_additionalCameraEnabled[fromIndex];
	this->m_additionalCameraPaths.remove_at(fromIndex);
	this->m_additionalCameraEnabled.remove_at(fromIndex);
	this->m_additionalCameraPaths.insert(toIndex, path);
	this->m_additionalCameraEnabled.insert(toIndex, enabled);
	return true;
}

bool CesiumCameraManager::set_camera_path(
	int32_t index,
	const NodePath& cameraPath
) {
	if (index < 0 || index >= this->m_additionalCameraPaths.size()) {
		return false;
	}
	this->m_additionalCameraPaths[index] = cameraPath;
	return true;
}

NodePath CesiumCameraManager::get_camera_path(int32_t index) const {
	if (index < 0 || index >= this->m_additionalCameraPaths.size()) {
		return NodePath();
	}
	const Variant value = this->m_additionalCameraPaths[index];
	return value.get_type() == Variant::NODE_PATH
		? static_cast<NodePath>(value)
		: NodePath();
}

bool CesiumCameraManager::set_camera_enabled(int32_t index, bool enabled) {
	if (index < 0 || index >= this->m_additionalCameraEnabled.size()) {
		return false;
	}
	this->m_additionalCameraEnabled[index] = enabled;
	return true;
}

bool CesiumCameraManager::get_camera_enabled(int32_t index) const {
	if (index < 0 || index >= this->m_additionalCameraEnabled.size()) {
		return false;
	}
	return static_cast<bool>(this->m_additionalCameraEnabled[index]);
}

int32_t CesiumCameraManager::get_camera_count() const {
	return this->m_additionalCameraPaths.size();
}

void CesiumCameraManager::clear_cameras() {
	this->m_additionalCameraPaths.clear();
	this->m_additionalCameraEnabled.clear();
}

CesiumCameraManager::Resolution CesiumCameraManager::resolve_cameras(
	Node3D* worldContext
) const {
	Resolution result;
	result.viewportCameraRequested = this->m_useViewportCamera;
	std::unordered_set<uint64_t> seen;
	auto appendCamera = [&](Camera3D* camera) {
		if (!same_world(worldContext, camera)) {
			++result.wrongWorld;
			return;
		}
		const uint64_t id = static_cast<uint64_t>(camera->get_instance_id());
		if (!seen.insert(id).second) {
			++result.duplicates;
			return;
		}
		result.cameras.push_back(camera);
	};

	if (this->m_useViewportCamera) {
		Viewport* viewport = worldContext == nullptr
			? nullptr
			: worldContext->get_viewport();
		Camera3D* viewportCamera = viewport == nullptr
			? nullptr
			: viewport->get_camera_3d();
		if (viewportCamera == nullptr) {
			++result.missingOrFreed;
		} else {
			const size_t previousCount = result.cameras.size();
			appendCamera(viewportCamera);
			result.viewportCameraResolved =
				result.cameras.size() > previousCount;
		}
	}

	for (
		int32_t index = 0;
		index < this->m_additionalCameraPaths.size();
		++index
	) {
		const bool enabled = index < this->m_additionalCameraEnabled.size()
			? static_cast<bool>(this->m_additionalCameraEnabled[index])
			: true;
		if (!enabled) {
			++result.disabled;
			continue;
		}
		++result.enabled;
		const Variant value = this->m_additionalCameraPaths[index];
		if (value.get_type() != Variant::NODE_PATH) {
			++result.wrongType;
			continue;
		}
		const NodePath path = static_cast<NodePath>(value);
		Node* node = path.is_empty() ? nullptr : this->get_node_or_null(path);
		if (node == nullptr) {
			++result.missingOrFreed;
			continue;
		}
		Camera3D* camera = Object::cast_to<Camera3D>(node);
		if (camera == nullptr) {
			++result.wrongType;
			continue;
		}
		appendCamera(camera);
	}
	return result;
}

Array CesiumCameraManager::get_resolved_cameras(Node3D* worldContext) const {
	const Resolution resolution = this->resolve_cameras(worldContext);
	Array result;
	for (Camera3D* camera : resolution.cameras) {
		result.push_back(camera);
	}
	return result;
}

Dictionary CesiumCameraManager::get_resolution_diagnostics(
	Node3D* worldContext
) const {
	const Resolution resolution = this->resolve_cameras(worldContext);
	Dictionary result;
	result["configured"] = this->m_additionalCameraPaths.size();
	result["enabled"] = resolution.enabled;
	result["disabled"] = resolution.disabled;
	result["resolved"] = static_cast<int64_t>(resolution.cameras.size());
	result["missing_or_freed"] = resolution.missingOrFreed;
	result["wrong_type"] = resolution.wrongType;
	result["wrong_world"] = resolution.wrongWorld;
	result["duplicates"] = resolution.duplicates;
	result["viewport_camera_requested"] = resolution.viewportCameraRequested;
	result["viewport_camera_resolved"] = resolution.viewportCameraResolved;
	return result;
}

void CesiumCameraManager::normalize_enabled_count() {
	while (
		this->m_additionalCameraEnabled.size() <
		this->m_additionalCameraPaths.size()
	) {
		this->m_additionalCameraEnabled.push_back(true);
	}
	while (
		this->m_additionalCameraEnabled.size() >
		this->m_additionalCameraPaths.size()
	) {
		this->m_additionalCameraEnabled.remove_at(
			this->m_additionalCameraEnabled.size() - 1
		);
	}
}

void CesiumCameraManager::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("set_use_viewport_camera", "use_viewport_camera"),
		&CesiumCameraManager::set_use_viewport_camera
	);
	ClassDB::bind_method(
		D_METHOD("get_use_viewport_camera"),
		&CesiumCameraManager::get_use_viewport_camera
	);
	ClassDB::bind_method(
		D_METHOD("set_additional_camera_paths", "paths"),
		&CesiumCameraManager::set_additional_camera_paths
	);
	ClassDB::bind_method(
		D_METHOD("get_additional_camera_paths"),
		&CesiumCameraManager::get_additional_camera_paths
	);
	ClassDB::bind_method(
		D_METHOD("set_additional_camera_enabled", "enabled"),
		&CesiumCameraManager::set_additional_camera_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_additional_camera_enabled"),
		&CesiumCameraManager::get_additional_camera_enabled
	);
	ClassDB::bind_method(
		D_METHOD("add_camera", "camera_path", "enabled"),
		&CesiumCameraManager::add_camera,
		DEFVAL(true)
	);
	ClassDB::bind_method(
		D_METHOD("remove_camera_at", "index"),
		&CesiumCameraManager::remove_camera_at
	);
	ClassDB::bind_method(
		D_METHOD("move_camera", "from_index", "to_index"),
		&CesiumCameraManager::move_camera
	);
	ClassDB::bind_method(
		D_METHOD("set_camera_path", "index", "camera_path"),
		&CesiumCameraManager::set_camera_path
	);
	ClassDB::bind_method(
		D_METHOD("get_camera_path", "index"),
		&CesiumCameraManager::get_camera_path
	);
	ClassDB::bind_method(
		D_METHOD("set_camera_enabled", "index", "enabled"),
		&CesiumCameraManager::set_camera_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_camera_enabled", "index"),
		&CesiumCameraManager::get_camera_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_camera_count"),
		&CesiumCameraManager::get_camera_count
	);
	ClassDB::bind_method(
		D_METHOD("clear_cameras"),
		&CesiumCameraManager::clear_cameras
	);
	ClassDB::bind_method(
		D_METHOD("get_resolved_cameras", "world_context"),
		&CesiumCameraManager::get_resolved_cameras
	);
	ClassDB::bind_method(
		D_METHOD("get_resolution_diagnostics", "world_context"),
		&CesiumCameraManager::get_resolution_diagnostics
	);

	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "use_viewport_camera"),
		"set_use_viewport_camera",
		"get_use_viewport_camera"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::ARRAY, "additional_camera_paths"),
		"set_additional_camera_paths",
		"get_additional_camera_paths"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::ARRAY, "additional_camera_enabled"),
		"set_additional_camera_enabled",
		"get_additional_camera_enabled"
	);
}
