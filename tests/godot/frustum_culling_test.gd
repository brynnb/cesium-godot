extends SceneTree

const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600
const EXPECTED_ASPECT := 640.0 / 360.0
const EPSILON := 0.00001


class TrackingReceiver extends Cesium3DTilesetLifecycleEventReceiver:
	var load_count: Dictionary = {}
	var visible: Dictionary = {}

	func _on_tile_loaded(tile: Cesium3DTile) -> void:
		var side := str((tile.tile_extras as Dictionary).get("side", ""))
		if side.is_empty():
			return
		load_count[side] = int(load_count.get(side, 0)) + 1

	func _on_tile_visibility_changed(
		tile: Cesium3DTile,
		is_visible: bool
	) -> void:
		var side := str((tile.tile_extras as Dictionary).get("side", ""))
		if not side.is_empty():
			visible[side] = is_visible


var test_root: Node3D
var test_viewport: SubViewport
var camera: Camera3D
var georeference: CesiumGeoreference
var tileset: Cesium3DTileset
var receiver: TrackingReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_viewport = SubViewport.new()
	test_viewport.size = Vector2i(640, 360)
	test_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(test_viewport)
	test_root = Node3D.new()
	test_root.name = "FrustumCullingTest"
	test_viewport.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.set_perspective(60.0, 0.1, 100_000.0)
	camera.keep_aspect = Camera3D.KEEP_WIDTH
	camera.position = Vector3(EARTH_RADIUS + 50.0, 0.0, 0.0)
	test_root.add_child(camera)
	_look_forward()

	georeference = CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = TrackingReceiver.new()
	test_root.add_child(receiver)
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/culling/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 3
	tileset.maximum_cached_bytes = 16 * 1024 * 1024
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.movement_prediction_enabled = false
	tileset.frustum_culling_enabled = true
	tileset.fog_culling_enabled = false
	tileset.render_tiles_under_camera = false
	tileset.http_cache_enabled = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	if not await _wait_visible("front"):
		_fail(
			"The centered front tile never became visible: loads=%s visible=%s stats=%s"
			% [receiver.load_count, receiver.visible, tileset.get_streaming_statistics()]
		)
		return
	if int(receiver.load_count.get("back", 0)) != 0:
		_fail("The tile behind the camera loaded with frustum culling enabled")
		return
	if int(receiver.load_count.get("edge", 0)) != 0:
		_fail("KEEP_WIDTH used a vertical FOV and loaded the edge tile")
		return

	var narrow := tileset.get_streaming_statistics()
	var expected_vertical := 2.0 * atan(
		tan(deg_to_rad(60.0) * 0.5) / EXPECTED_ASPECT
	)
	if (
		not bool(narrow.view_state_valid) or
		str(narrow.view_projection_type_name) != "perspective" or
		not bool(narrow.view_keep_width) or
		absf(float(narrow.view_horizontal_fov_radians) - deg_to_rad(60.0)) >
			EPSILON or
		absf(float(narrow.view_vertical_fov_radians) - expected_vertical) >
			EPSILON or
		int(narrow.tiles_culled) < 2 or
		bool(narrow.occlusion_culling_available) or
		bool(narrow.occlusion_culling_enabled) or
		str(narrow.occlusion_culling_backend) != "unavailable" or
		str(narrow.occlusion_culling_unavailable_reason).is_empty()
	):
		_fail("KEEP_WIDTH/culling diagnostics were incorrect: %s" % [narrow])
		return
	var base_eye: PackedFloat64Array = narrow.view_position_components
	camera.h_offset = 25.0
	if not is_equal_approx(camera.h_offset, 25.0):
		_fail("Godot did not retain Camera3D.h_offset")
		return
	_update_once()
	var offset_stats := tileset.get_streaming_statistics()
	var offset_eye: PackedFloat64Array = offset_stats.view_position_components
	if (
		base_eye.size() != 3 or offset_eye.size() != 3 or
		absf(Vector3(
			offset_eye[0] - base_eye[0],
			offset_eye[1] - base_eye[1],
			offset_eye[2] - base_eye[2]
		).length() - 25.0) > EPSILON
	):
		_fail("Camera h_offset did not move the Cesium selection eye: %s" % [offset_stats])
		return
	camera.h_offset = 0.0
	_update_once()

	# The same 60-degree authored FOV is vertical in KEEP_HEIGHT mode, making
	# the horizontal field wide enough to include the tile at roughly 40 degrees.
	camera.keep_aspect = Camera3D.KEEP_HEIGHT
	if not await _wait_visible("edge"):
		_fail("KEEP_HEIGHT did not widen the horizontal frustum")
		return
	var wide := tileset.get_streaming_statistics()
	var expected_horizontal := 2.0 * atan(
		EXPECTED_ASPECT * tan(deg_to_rad(60.0) * 0.5)
	)
	if (
		bool(wide.view_keep_width) or
		absf(float(wide.view_vertical_fov_radians) - deg_to_rad(60.0)) >
			EPSILON or
		absf(float(wide.view_horizontal_fov_radians) - expected_horizontal) >
			EPSILON
	):
		_fail("KEEP_HEIGHT projection diagnostics were incorrect: %s" % [wide])
		return

	# Turning away should retain the old content in the Native LRU, not unload or
	# reload it. Turning back should reveal the same realized tile immediately.
	camera.keep_aspect = Camera3D.KEEP_WIDTH
	_look_backward()
	if not await _wait_visible("back"):
		_fail("The back tile did not stream after the camera turn")
		return
	if bool(receiver.visible.get("front", false)):
		_fail("The front tile remained visible behind the camera")
		return
	_look_forward()
	if not await _wait_visible("front"):
		_fail("The retained front tile did not reappear")
		return
	if int(receiver.load_count.get("front", 0)) != 1:
		_fail("Camera turnaround reloaded a retained tile")
		return

	# Disabling frustum culling is live and makes the behind-camera leaf eligible
	# without recreating the tileset.
	tileset.enforce_culled_screen_space_error = false
	tileset.culled_screen_space_error = 48.0
	tileset.frustum_culling_enabled = false
	if not await _wait_visible("back"):
		_fail("Disabling frustum culling did not select the back tile")
		return
	var uncull := tileset.get_streaming_statistics()
	if (
		bool(uncull.frustum_culling_enabled) or
		bool(uncull.enforce_culled_screen_space_error) or
		not is_equal_approx(float(uncull.culled_screen_space_error), 48.0) or
		bool(uncull.render_tiles_under_camera)
	):
		_fail("Live culling controls were not reflected in Native options: %s" % [uncull])
		return

	if not _check_orthographic_projection():
		return
	if not _check_asymmetric_frustum_projection():
		return

	print("Cesium Godot camera projection and frustum-culling tests passed")
	test_root.free()
	test_viewport.queue_free()
	for _frame in range(4):
		await process_frame
	quit(0)


func _check_orthographic_projection() -> bool:
	camera.keep_aspect = Camera3D.KEEP_HEIGHT
	camera.set_orthogonal(200.0, 0.5, 100_000.0)
	_update_once()
	var stats := tileset.get_streaming_statistics()
	var extents: Vector4 = stats.view_plane_extents
	if (
		str(stats.view_projection_type_name) != "orthogonal" or
		absf(extents.x + 100.0 * EXPECTED_ASPECT) > EPSILON or
		absf(extents.y - 100.0 * EXPECTED_ASPECT) > EPSILON or
		absf(extents.z + 100.0) > EPSILON or
		absf(extents.w - 100.0) > EPSILON
	):
		_fail("Orthographic KEEP_HEIGHT extents were incorrect: %s" % [stats])
		return false
	return true


func _check_asymmetric_frustum_projection() -> bool:
	camera.keep_aspect = Camera3D.KEEP_WIDTH
	camera.set_frustum(100.0, Vector2(25.0, -10.0), 0.5, 100_000.0)
	_update_once()
	var stats := tileset.get_streaming_statistics()
	var extents: Vector4 = stats.view_plane_extents
	var half_height := 50.0 / EXPECTED_ASPECT
	if (
		str(stats.view_projection_type_name) != "frustum" or
		absf(extents.x + 25.0) > EPSILON or
		absf(extents.y - 75.0) > EPSILON or
		absf(extents.z - (-half_height - 10.0)) > EPSILON or
		absf(extents.w - (half_height - 10.0)) > EPSILON or
		absf(float(stats.view_near_plane) - 0.5) > EPSILON
	):
		_fail("Asymmetric frustum extents were incorrect: %s" % [stats])
		return false
	return true


func _wait_visible(side: String) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		_update_once()
		await process_frame
		if bool(receiver.visible.get(side, false)):
			return true
	return false


func _update_once() -> void:
	tileset.update_tileset(camera.global_transform)


func _look_forward() -> void:
	camera.look_at(
		Vector3(EARTH_RADIUS + 50.0, 0.0, 1000.0),
		Vector3.UP
	)


func _look_backward() -> void:
	camera.look_at(
		Vector3(EARTH_RADIUS + 50.0, 0.0, -1000.0),
		Vector3.UP
	)


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	if test_viewport != null and is_instance_valid(test_viewport):
		test_viewport.queue_free()
	quit(1)
