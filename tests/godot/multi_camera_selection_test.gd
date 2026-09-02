extends SceneTree

# Contract exercised by this forward-looking integration test:
#
# - CesiumCameraManager is a Node with a viewport-camera switch and an ordered
#   list of optional Camera3D NodePaths.
# - Cesium3DTileset.camera_manager_path resolves that manager every frame.
# - all valid, enabled cameras in the same World3D contribute render views to
#   one Cesium Native selection update.
#
# ClassDB and dynamic property/method access keep this script parseable on the
# pre-feature ABI. Missing pieces fail with a precise contract message instead
# of an "unknown class" parser error.

const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600
const PROJECTION_EPSILON := 0.001


class TrackingReceiver extends Cesium3DTilesetLifecycleEventReceiver:
	var load_count: Dictionary = {}
	var visible: Dictionary = {}

	func _init() -> void:
		tile_loaded.connect(_on_tile_loaded)
		tile_visibility_changed.connect(_on_tile_visibility_changed)

	func _on_tile_loaded(tile: Cesium3DTile) -> void:
		var view_name := str((tile.tile_extras as Dictionary).get("view", ""))
		if not view_name.is_empty():
			load_count[view_name] = int(load_count.get(view_name, 0)) + 1

	func _on_tile_visibility_changed(
		tile: Cesium3DTile,
		is_visible: bool
	) -> void:
		var view_name := str((tile.tile_extras as Dictionary).get("view", ""))
		if not view_name.is_empty():
			visible[view_name] = is_visible


var test_viewport: SubViewport
var test_root: Node3D
var auxiliary_viewport: SubViewport
var auxiliary_root: Node3D
var wrong_world_viewport: SubViewport
var wrong_world_root: Node3D
var primary_camera: Camera3D
var auxiliary_camera: Camera3D
var wrong_world_camera: Camera3D
var camera_manager: Node
var wrong_type_node: Node3D
var georeference: CesiumGeoreference
var tileset: Cesium3DTileset
var receiver: TrackingReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not ClassDB.class_exists(&"CesiumCameraManager"):
		_fail_contract(
			"CesiumCameraManager is not registered; the multi-camera runtime " +
			"contract has not been implemented"
		)
		return

	test_viewport = SubViewport.new()
	test_viewport.name = "MultiCameraViewport"
	test_viewport.size = Vector2i(640, 360)
	test_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(test_viewport)
	test_root = Node3D.new()
	test_root.name = "MultiCameraSelectionTest"
	test_viewport.add_child(test_root)

	primary_camera = _create_camera(
		test_root,
		"PrimaryCamera",
		true,
		1.0,
		false
	)

	# The auxiliary camera deliberately renders through its own, smaller
	# SubViewport and uses an orthogonal projection. Sharing World3D makes this a
	# valid secondary view while still exercising the camera's own viewport size
	# and projection rather than the tileset viewport's perspective settings.
	auxiliary_viewport = SubViewport.new()
	auxiliary_viewport.name = "AuxiliaryViewport"
	auxiliary_viewport.size = Vector2i(320, 180)
	auxiliary_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(auxiliary_viewport)
	auxiliary_viewport.world_3d = test_root.get_world_3d()
	auxiliary_root = Node3D.new()
	auxiliary_root.name = "AuxiliaryWorldRoot"
	auxiliary_viewport.add_child(auxiliary_root)
	auxiliary_camera = _create_camera(
		auxiliary_root,
		"AuxiliaryCamera",
		true,
		-1.0,
		true
	)

	# This camera has intentionally similar coordinates but belongs to a
	# different World3D. It is added later to prove that manager resolution does
	# not leak cameras across independent render worlds.
	wrong_world_viewport = SubViewport.new()
	wrong_world_viewport.name = "WrongWorldViewport"
	wrong_world_viewport.size = Vector2i(256, 256)
	wrong_world_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(wrong_world_viewport)
	wrong_world_viewport.world_3d = World3D.new()
	wrong_world_root = Node3D.new()
	wrong_world_root.name = "WrongWorldRoot"
	wrong_world_viewport.add_child(wrong_world_root)
	wrong_world_camera = _create_camera(
		wrong_world_root,
		"WrongWorldCamera",
		true,
		-1.0,
		false
	)
	wrong_type_node = Node3D.new()
	wrong_type_node.name = "NotACameraOrManager"
	test_root.add_child(wrong_type_node)

	var manager_object: Object = ClassDB.instantiate(&"CesiumCameraManager")
	camera_manager = manager_object as Node
	if camera_manager == null:
		_fail_contract("CesiumCameraManager could not be instantiated as a Node")
		return
	camera_manager.name = "CameraManager"
	test_root.add_child(camera_manager)
	if not _require_manager_contract():
		return
	camera_manager.call("set_use_viewport_camera", true)
	var auxiliary_index := int(camera_manager.call(
		"add_camera",
		camera_manager.get_path_to(auxiliary_camera),
		true
	))
	if auxiliary_index != 0:
		_fail_contract(
			"CesiumCameraManager.add_camera must return the inserted index; got %d"
			% auxiliary_index
		)
		return

	georeference = CesiumGeoreference.new()
	georeference.name = "TrueOrigin"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = TrackingReceiver.new()
	receiver.name = "TrackingReceiver"
	test_root.add_child(receiver)
	tileset = Cesium3DTileset.new()
	tileset.name = "MultiCameraTileset"
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/multi_camera/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.maximum_cached_bytes = 16 * 1024 * 1024
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.movement_prediction_enabled = false
	tileset.frustum_culling_enabled = true
	tileset.fog_culling_enabled = false
	tileset.http_cache_enabled = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)
	if not _has_property(tileset, &"camera_manager_path"):
		_fail_contract(
			"Cesium3DTileset.camera_manager_path is missing; the manager cannot " +
			"participate in tile selection"
		)
		return
	tileset.set(
		"camera_manager_path",
		tileset.get_path_to(camera_manager)
	)

	if not await _wait_for_visibility(true, true):
		_fail(
			"The two disjoint camera views were not selected together: " +
			"loads=%s visible=%s stats=%s manager=%s"
			% [
				receiver.load_count,
				receiver.visible,
				tileset.get_streaming_statistics(),
				_manager_diagnostics(),
			]
		)
		return
	if not _check_selection_diagnostics(2, 2, 0, "initial two-camera selection"):
		return
	if int(receiver.load_count.get("primary", 0)) != 1:
		_fail("The primary leaf did not load exactly once")
		return
	if int(receiver.load_count.get("auxiliary", 0)) != 1:
		_fail("The auxiliary leaf did not load exactly once")
		return

	# Making the auxiliary camera the sole selection view must expose its own
	# orthogonal projection and 320x180 SubViewport in the primary-view
	# diagnostics. This catches implementations that gather camera pointers but
	# accidentally reuse the player viewport or projection for every view.
	camera_manager.call("set_use_viewport_camera", false)
	if not await _wait_for_auxiliary_visibility(true):
		_fail(
			"The auxiliary orthogonal camera could not select by itself: " +
			"visible=%s stats=%s manager=%s"
			% [
				receiver.visible,
				tileset.get_streaming_statistics(),
				_manager_diagnostics(),
			]
		)
		return
	if not _check_selection_diagnostics(
		1,
		1,
		0,
		"sole auxiliary orthogonal camera"
	):
		return
	var auxiliary_stats := tileset.get_streaming_statistics()
	var auxiliary_viewport_size: Vector2 = auxiliary_stats.get(
		"view_viewport_size",
		Vector2.ZERO
	)
	if (
		str(auxiliary_stats.get("view_projection_type_name", "")) !=
			"orthogonal" or
		auxiliary_viewport_size.distance_to(Vector2(320.0, 180.0)) >
			PROJECTION_EPSILON
	):
		_fail(
			"The auxiliary camera did not retain its orthogonal projection " +
			"and SubViewport size: %s" % [auxiliary_stats]
		)
		return
	camera_manager.call("set_use_viewport_camera", true)
	if not await _wait_for_visibility(true, true):
		_fail("Restoring the viewport camera did not restore both selections")
		return

	# Duplicate, wrong-type, and wrong-world entries are rejected independently.
	# None may create a third selection view or disturb the two valid cameras.
	var duplicate_index := int(camera_manager.call(
		"add_camera",
		camera_manager.get_path_to(auxiliary_camera),
		true
	))
	var wrong_type_index := int(camera_manager.call(
		"add_camera",
		camera_manager.get_path_to(wrong_type_node),
		true
	))
	var wrong_world_index := int(camera_manager.call(
		"add_camera",
		camera_manager.get_path_to(wrong_world_camera),
		true
	))
	if (
		duplicate_index != 1 or
		wrong_type_index != 2 or
		wrong_world_index != 3
	):
		_fail("Adversarial camera entries received unexpected list indices")
		return
	_pump_once()
	await process_frame
	if not _check_selection_diagnostics(
		2,
		2,
		1,
		"duplicate, wrong-type, and wrong-world entries",
		1,
		1
	):
		return
	var adversarial_diagnostics := _manager_diagnostics()
	if (
		int(adversarial_diagnostics.get("configured", -1)) != 4 or
		int(adversarial_diagnostics.get("enabled", -1)) != 4 or
		int(adversarial_diagnostics.get("resolved", -1)) != 2 or
		int(adversarial_diagnostics.get("wrong_type", -1)) != 1 or
		int(adversarial_diagnostics.get("wrong_world", -1)) != 1 or
		int(adversarial_diagnostics.get("duplicates", -1)) != 1
	):
		_fail(
			"Manager rejection diagnostics were incomplete: %s"
			% [adversarial_diagnostics]
		)
		return
	# Remove in descending order so the original auxiliary remains index zero.
	for index in [wrong_world_index, wrong_type_index, duplicate_index]:
		if not bool(camera_manager.call("remove_camera_at", index)):
			_fail("Could not remove adversarial camera entry %d" % index)
			return
	if int(camera_manager.call("get_camera_count")) != 1:
		_fail("Removing adversarial entries disturbed the real auxiliary entry")
		return

	# Disabling an entry is live. It stops contributing to selection without
	# deleting the Camera3D or recreating the tileset.
	if not bool(camera_manager.call(
		"set_camera_enabled",
		auxiliary_index,
		false
	)):
		_fail("set_camera_enabled rejected the valid auxiliary index")
		return
	if not await _wait_for_visibility(true, false):
		_fail("Disabling the auxiliary camera left its leaf visible")
		return
	if not _check_selection_diagnostics(1, 1, 0, "disabled auxiliary camera"):
		return
	var disabled_diagnostics := _manager_diagnostics()
	if (
		int(disabled_diagnostics.get("enabled", -1)) != 0 or
		int(disabled_diagnostics.get("disabled", -1)) != 1
	):
		_fail(
			"Manager disable diagnostics were incorrect: %s"
			% [disabled_diagnostics]
		)
		return

	# With both the viewport camera and the only additional entry disabled, the
	# manager intentionally contributes zero views. Native selection and realized
	# Godot visuals must both become empty instead of leaving the previous camera's
	# tiles stale on screen.
	camera_manager.call("set_use_viewport_camera", false)
	if not await _wait_for_visibility(false, false):
		_fail(
			"Zero managed cameras left stale tiles visible: visible=%s stats=%s"
			% [receiver.visible, tileset.get_streaming_statistics()]
		)
		return
	if not _check_selection_diagnostics(0, 0, 0, "zero enabled cameras"):
		return
	var zero_view_stats := tileset.get_streaming_statistics()
	if (
		bool(zero_view_stats.get("view_state_valid", true)) or
		str(zero_view_stats.get("view_projection_type_name", "")) !=
			"unavailable"
	):
		_fail("Zero-camera primary-view diagnostics were stale: %s" % [zero_view_stats])
		return
	camera_manager.call("set_use_viewport_camera", true)
	if not await _wait_for_visibility(true, false):
		_fail("Restoring the viewport camera after zero views failed")
		return

	# Re-enabling uses the retained native tile. Selection should not reload the
	# same content while it remains inside the cache budget.
	if not bool(camera_manager.call(
		"set_camera_enabled",
		auxiliary_index,
		true
	)):
		_fail("Re-enabling the auxiliary camera failed")
		return
	if not await _wait_for_visibility(true, true):
		_fail("Re-enabling the auxiliary camera did not restore its leaf")
		return
	if int(receiver.load_count.get("auxiliary", 0)) != 1:
		_fail("Re-enabling a retained auxiliary leaf reloaded its content")
		return

	# The manager owns a NodePath rather than a raw pointer. Freeing a referenced
	# camera must become one invalid diagnostic entry and must never dereference
	# stale memory.
	auxiliary_camera.free()
	auxiliary_camera = null
	if not await _wait_for_visibility(true, false):
		_fail("Freeing the auxiliary camera left its leaf selected")
		return
	if not _check_selection_diagnostics(1, 1, 1, "freed auxiliary camera"):
		return
	var freed_diagnostics := _manager_diagnostics()
	if int(freed_diagnostics.get("missing_or_freed", -1)) != 1:
		_fail(
			"A freed camera was not reported as missing_or_freed: %s"
			% [freed_diagnostics]
		)
		return

	# Removing the stale entry clears the invalid count while preserving the
	# viewport camera and primary selection.
	if not bool(camera_manager.call("remove_camera_at", auxiliary_index)):
		_fail("remove_camera_at rejected the stale but valid list index")
		return
	_pump_once()
	await process_frame
	if int(camera_manager.call("get_camera_count")) != 0:
		_fail("remove_camera_at did not remove the auxiliary entry")
		return
	if not _check_selection_diagnostics(1, 1, 0, "removed auxiliary camera"):
		return
	if not bool(receiver.visible.get("primary", false)):
		_fail("Removing the auxiliary entry disturbed primary-camera selection")
		return
	var managed_primary_stats := tileset.get_streaming_statistics()

	# A manager path that resolves to the wrong class (and a stale path that
	# resolves to nothing) must be diagnosed but fall back to the legacy current
	# viewport camera. A bad optional manager cannot blank the player's world.
	tileset.set(
		"camera_manager_path",
		tileset.get_path_to(wrong_type_node)
	)
	if not await _wait_for_visibility(true, false):
		_fail("A wrong-type manager path suppressed the viewport fallback")
		return
	if not _check_selection_diagnostics(
		1,
		1,
		1,
		"wrong-type manager path fallback",
		0,
		0,
		true,
		false
	):
		return

	tileset.set(
		"camera_manager_path",
		NodePath("MissingCameraManager")
	)
	if not await _wait_for_visibility(true, false):
		_fail("A missing manager path suppressed the viewport fallback")
		return
	if not _check_selection_diagnostics(
		1,
		1,
		1,
		"missing manager path fallback",
		0,
		0,
		true,
		false
	):
		return

	# Finally clear the manager path entirely and compare the legacy one-camera
	# projection with the manager's one-camera result. Both routes must produce
	# the same player view rather than subtly changing field of view or viewport.
	tileset.set("camera_manager_path", NodePath())
	if not await _wait_for_visibility(true, false):
		_fail("Clearing camera_manager_path broke legacy one-camera selection")
		return
	if not _check_selection_diagnostics(
		1,
		1,
		0,
		"legacy one-camera selection",
		0,
		0,
		false,
		false
	):
		return
	var legacy_stats := tileset.get_streaming_statistics()
	if not _check_legacy_equivalence(managed_primary_stats, legacy_stats):
		return

	print(
		"Cesium multi-camera selection test passed: loads=",
		receiver.load_count,
		" manager=",
		_manager_diagnostics()
	)
	_cleanup()
	quit(0)


func _create_camera(
	parent: Node3D,
	camera_name: String,
	make_current: bool,
	look_direction_z: float,
	orthogonal: bool
) -> Camera3D:
	var result := Camera3D.new()
	result.name = camera_name
	result.current = make_current
	if orthogonal:
		result.set_orthogonal(1200.0, 0.1, 100_000.0)
	else:
		result.set_perspective(60.0, 0.1, 100_000.0)
	result.keep_aspect = Camera3D.KEEP_WIDTH
	result.position = Vector3(EARTH_RADIUS + 50.0, 0.0, 0.0)
	parent.add_child(result)
	result.look_at(
		Vector3(EARTH_RADIUS + 50.0, 0.0, look_direction_z * 1000.0),
		Vector3.UP
	)
	return result


func _require_manager_contract() -> bool:
	var methods := [
		&"set_use_viewport_camera",
		&"add_camera",
		&"remove_camera_at",
		&"set_camera_enabled",
		&"get_camera_count",
		&"get_resolution_diagnostics",
	]
	for method_name: StringName in methods:
		if not camera_manager.has_method(method_name):
			_fail_contract(
				"CesiumCameraManager is missing required method %s"
				% method_name
			)
			return false
	return true


func _has_property(object: Object, property_name: StringName) -> bool:
	for property_info: Dictionary in object.get_property_list():
		if StringName(property_info.get("name", "")) == property_name:
			return true
	return false


func _wait_for_visibility(
	primary_visible: bool,
	auxiliary_visible: bool
) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		_pump_once()
		await process_frame
		if (
			bool(receiver.visible.get("primary", false)) == primary_visible and
			bool(receiver.visible.get("auxiliary", false)) == auxiliary_visible
		):
			return true
	return false


func _wait_for_auxiliary_visibility(auxiliary_visible: bool) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		_pump_once()
		await process_frame
		if bool(receiver.visible.get("auxiliary", false)) == auxiliary_visible:
			return true
	return false


func _pump_once() -> void:
	# The transform remains the backwards-compatible explicit-camera argument.
	# With camera_manager_path assigned, the tileset resolves every configured
	# view and performs a single multi-view Native selection update.
	tileset.update_tileset(primary_camera.global_transform)


func _manager_diagnostics() -> Dictionary:
	if camera_manager == null or not is_instance_valid(camera_manager):
		return {}
	return camera_manager.call("get_resolution_diagnostics", test_root) as Dictionary


func _check_selection_diagnostics(
	expected_views: int,
	expected_render_views: int,
	expected_invalid: int,
	context: String,
	expected_wrong_world: int = 0,
	expected_duplicates: int = 0,
	expected_manager_configured: bool = true,
	expected_manager_resolved: bool = true
) -> bool:
	var stats := tileset.get_streaming_statistics()
	var required_keys := [
		"selection_view_count",
		"selection_render_view_count",
		"selection_invalid_camera_count",
		"selection_wrong_world_camera_count",
		"selection_duplicate_camera_count",
		"selection_camera_manager_configured",
		"selection_camera_manager_resolved",
	]
	for key: String in required_keys:
		if not stats.has(key):
			_fail_contract(
				"Streaming statistics are missing %s during %s"
				% [key, context]
			)
			return false
	if (
		int(stats.get("selection_view_count", -1)) != expected_views or
		int(stats.get("selection_render_view_count", -1)) !=
			expected_render_views or
		int(stats.get("selection_invalid_camera_count", -1)) !=
			expected_invalid or
		int(stats.get("selection_wrong_world_camera_count", -1)) !=
			expected_wrong_world or
		int(stats.get("selection_duplicate_camera_count", -1)) !=
			expected_duplicates or
		bool(stats.get("selection_camera_manager_configured", false)) !=
			expected_manager_configured or
		bool(stats.get("selection_camera_manager_resolved", false)) !=
			expected_manager_resolved
	):
		_fail(
			"Selection diagnostics were incorrect during %s: %s"
			% [context, stats]
		)
		return false
	return true


func _check_legacy_equivalence(
	managed_stats: Dictionary,
	legacy_stats: Dictionary
) -> bool:
	var exact_keys := [
		"view_state_valid",
		"view_projection_type",
		"view_projection_type_name",
		"view_keep_width",
		"view_viewport_size",
		"view_position_components",
		"view_direction_components",
		"view_up_components",
		"view_plane_extents",
	]
	for key: String in exact_keys:
		if managed_stats.get(key) != legacy_stats.get(key):
			_fail(
				"Legacy and managed one-camera selection differ for %s: " +
				"managed=%s legacy=%s"
				% [key, managed_stats.get(key), legacy_stats.get(key)]
			)
			return false
	var approximate_keys := [
		"view_horizontal_fov_radians",
		"view_vertical_fov_radians",
		"view_near_plane",
	]
	for key: String in approximate_keys:
		if absf(
			float(managed_stats.get(key, 0.0)) -
			float(legacy_stats.get(key, 0.0))
		) > PROJECTION_EPSILON:
			_fail(
				"Legacy and managed one-camera projection differ for %s: " +
				"managed=%s legacy=%s"
				% [key, managed_stats.get(key), legacy_stats.get(key)]
			)
			return false
	return true


func _fail_contract(message: String) -> void:
	_fail("Multi-camera API contract unavailable: " + message)


func _fail(message: String) -> void:
	push_error(message)
	_cleanup()
	quit(1)


func _cleanup() -> void:
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	if test_viewport != null and is_instance_valid(test_viewport):
		test_viewport.queue_free()
	if auxiliary_viewport != null and is_instance_valid(auxiliary_viewport):
		auxiliary_viewport.queue_free()
	if wrong_world_viewport != null and is_instance_valid(wrong_world_viewport):
		wrong_world_viewport.queue_free()
