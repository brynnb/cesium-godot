# Multi-camera runtime streaming

`CesiumCameraManager` lets one `Cesium3DTileset` select tiles for more than one
runtime `Camera3D` at once. Typical uses are mirrors, portals, minimaps, split
screen, and off-screen render targets. This is a game/runtime API; it does not
depend on the Godot editor plugin.

Without a manager, `Cesium3DTileset` keeps its existing behavior and selects
from the current camera of its viewport. Assigning a manager is explicit:

```gdscript
var camera_manager := CesiumCameraManager.new()
add_child(camera_manager)
camera_manager.use_viewport_camera = true
camera_manager.add_camera(
	camera_manager.get_path_to(mirror_camera),
	true
)
tileset.camera_manager_path = tileset.get_path_to(camera_manager)
```

The current viewport camera is ordered first when enabled. Additional cameras
retain their own perspective, orthographic, or asymmetric-frustum projection,
camera offsets, and viewport dimensions. All valid cameras are submitted in one
Cesium Native render view group, so a tile visible to any camera remains
selected at the strictest screen-space error demanded by those views. Cesium
for Unreal v2.29 likewise does not define a per-camera quality or priority
weight for render views.

Additional cameras must share the tileset's `World3D`. For a camera in a
`SubViewport`, explicitly share the world when it is intended to render the
same scene. Wrong-world, missing, wrong-type, and duplicate paths are ignored
and counted in `get_resolution_diagnostics()` and the tileset's streaming
statistics.

Camera entries are `NodePath`s, not retained object pointers. They resolve each
selection update, so freeing a camera safely turns its entry into a diagnostic
failure. `set_camera_enabled`, `set_camera_path`, `move_camera`,
`remove_camera_at`, and `clear_cameras` modify the live list without recreating
the Native tileset. Disabling every valid camera hides realized visuals and
collision while allowing Native's bounded cache to retain them.

Movement, turn, and zoom prediction remain a separate, lower-weight prefetch
view. Only the first valid render camera drives prediction, and predictor
history resets when that primary camera changes. The existing singular
`view_*` streaming statistics also describe that primary camera. The
`selection_render_view_count` field counts render cameras;
`selection_view_count` additionally includes an active predictive view.
