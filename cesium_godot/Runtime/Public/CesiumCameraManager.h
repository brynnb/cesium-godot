// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumCameraManager.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_CAMERA_MANAGER_H
#define CESIUM_CAMERA_MANAGER_H

#if defined(CESIUM_GD_MODULE)
#include "core/string/node_path.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/node_3d.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
using namespace godot;
#endif

#include <cstdint>
#include <vector>

/**
 * Collects the cameras that participate in one Cesium Native tile-selection
 * update. Entries are persisted as NodePaths and resolved every frame, so
 * freeing or replacing a Camera3D never leaves a retained engine pointer.
 *
 * Unlike the editor-centric parts of Cesium for Unreal's manager, this class
 * is a small runtime Node. It can include the current viewport camera and any
 * number of additional cameras (mirrors, minimaps, portals, or render targets).
 */
class CesiumCameraManager : public Node {
	GDCLASS(CesiumCameraManager, Node)

public:
	struct Resolution final {
		std::vector<Camera3D*> cameras;
		int32_t enabled = 0;
		int32_t disabled = 0;
		int32_t missingOrFreed = 0;
		int32_t wrongType = 0;
		int32_t wrongWorld = 0;
		int32_t duplicates = 0;
		bool viewportCameraRequested = false;
		bool viewportCameraResolved = false;

		int32_t invalid_count() const {
			return this->missingOrFreed + this->wrongType;
		}
	};

	void set_use_viewport_camera(bool useViewportCamera);
	bool get_use_viewport_camera() const;

	void set_additional_camera_paths(const Array& paths);
	Array get_additional_camera_paths() const;
	void set_additional_camera_enabled(const Array& enabled);
	Array get_additional_camera_enabled() const;

	int32_t add_camera(const NodePath& cameraPath, bool enabled = true);
	bool remove_camera_at(int32_t index);
	bool move_camera(int32_t fromIndex, int32_t toIndex);
	bool set_camera_path(int32_t index, const NodePath& cameraPath);
	NodePath get_camera_path(int32_t index) const;
	bool set_camera_enabled(int32_t index, bool enabled);
	bool get_camera_enabled(int32_t index) const;
	int32_t get_camera_count() const;
	void clear_cameras();

	Resolution resolve_cameras(Node3D* worldContext) const;
	Array get_resolved_cameras(Node3D* worldContext) const;
	Dictionary get_resolution_diagnostics(Node3D* worldContext) const;

protected:
	static void _bind_methods();

private:
	void normalize_enabled_count();

	bool m_useViewportCamera = true;
	Array m_additionalCameraPaths;
	Array m_additionalCameraEnabled;
};

#endif // CESIUM_CAMERA_MANAGER_H
