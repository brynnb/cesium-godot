extends SceneTree

const AdversarialReceiver := preload("res://adversarial_lifecycle_receiver.gd")
const EARTH_RADIUS := 6_378_137.0
const FRAME_TIMEOUT := 900

var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference
var tileset: Cesium3DTileset
var receiver: Cesium3DTilesetLifecycleEventReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "AdversarialLifecycleTest"
	root.add_child(test_root)
	camera = _create_camera(test_root)
	georeference = _create_georeference(test_root)
	receiver = AdversarialReceiver.new()
	receiver.name = "AdversarialReceiver"
	test_root.add_child(receiver)

	var fixture_url := "file://" + ProjectSettings.globalize_path(
		"res://fixtures/lifecycle/tileset.json"
	)
	tileset = _create_tileset(georeference, receiver, fixture_url)
	if not await _pump_until("visibility:true", 0, FRAME_TIMEOUT):
		_fail("Initial adversarial fixture did not become visible")
		return

	# A zero-byte Native tile cache must eventually evict an unselected tile,
	# with one valid pre-destruction unload callback.
	tileset.maximum_cached_bytes = 0
	var unload_index: int = receiver.events.size()
	_move_camera_away()
	if not await _pump_until("tile_unloading", unload_index, FRAME_TIMEOUT):
		_fail("Zero-byte cache did not evict the hidden fixture tile")
		return
	if receiver.events.has("invalid_tile_unloading"):
		_fail("Eviction delivered tile_unloading after the Godot tile was invalid")
		return

	# Restoring the view reloads the evicted content. Replacing the URL must
	# unload that generation and load a fresh one rather than retaining stale
	# Native or Godot resources.
	_move_camera_home()
	var reload_index: int = receiver.events.size()
	if not await _pump_until("tile_loaded", reload_index, FRAME_TIMEOUT):
		_fail("Evicted fixture did not reload")
		return
	var replacement_url := _write_replacement_tileset()
	if replacement_url.is_empty():
		_fail("Could not write the replacement tileset fixture")
		return
	unload_index = receiver.events.size()
	tileset.url = replacement_url
	if not await _pump_until("tile_unloading", unload_index, FRAME_TIMEOUT):
		_fail("Changing URL did not unload the previous tileset generation")
		return
	reload_index = receiver.events.size()
	if not await _pump_until("tile_loaded", reload_index, FRAME_TIMEOUT):
		_fail("Changing URL did not load the replacement tileset generation")
		return

	# Malformed render content is a permanent tile failure. It must not deliver
	# a partial lifecycle load, and the tileset must remain safe to destroy.
	var malformed_url := _write_malformed_tileset()
	if malformed_url.is_empty():
		_fail("Could not write the malformed tileset fixture")
		return
	var malformed_index: int = receiver.events.size()
	tileset.url = malformed_url
	await _pump_for_milliseconds(350)
	if receiver.events.find("tile_loaded", malformed_index) >= 0:
		_fail("Malformed glTF produced a partial tile_loaded callback")
		return
	tileset.free()
	tileset = null
	await process_frame

	# Godot cannot pause one RenderingServer mesh/texture upload once it starts.
	# An explicitly undersized per-primitive cap must reject the leaf before any
	# partial node or lifecycle callback is produced and report why in statistics.
	var oversized_receiver := AdversarialReceiver.new()
	oversized_receiver.name = "OversizedLeafReceiver"
	test_root.add_child(oversized_receiver)
	var oversized_tileset := _create_tileset(
		georeference,
		oversized_receiver,
		fixture_url
	)
	oversized_tileset.maximum_primitive_geometry_upload_bytes = 1
	for _frame in range(120):
		oversized_tileset.update_tileset(camera.global_transform)
		await process_frame
	var oversized_statistics := oversized_tileset.get_streaming_statistics()
	if (
		oversized_receiver.events.has("tile_loaded") or
		int(oversized_statistics.oversized_payload_rejections) < 1
	):
		_fail("Oversized indivisible payload was not rejected: %s" % [oversized_statistics])
		return
	oversized_tileset.free()
	oversized_tileset = null
	await process_frame

	# Cancel after at least one main-thread surface has been realized but before
	# the atomic tile completion. Partially-created Godot resources remain owned
	# by the worker result and must disappear without a tile_loaded callback.
	var partial_receiver := AdversarialReceiver.new()
	partial_receiver.name = "PartialRealizationReceiver"
	test_root.add_child(partial_receiver)
	var partial_tileset := _create_tileset(
		georeference,
		partial_receiver,
		fixture_url
	)
	partial_tileset.main_thread_loading_time_limit_ms = 0.001
	var partial_yielded := false
	for _frame in range(120):
		partial_tileset.update_tileset(camera.global_transform)
		await process_frame
		var partial_statistics := partial_tileset.get_streaming_statistics()
		if int(partial_statistics.incremental_realization_yields) > 0:
			partial_yielded = true
			break
	if not partial_yielded or partial_receiver.events.has("tile_loaded"):
		_fail("Could not pause the fixture during incremental realization")
		return
	partial_tileset.free()
	partial_tileset = null
	for _frame in range(8):
		await process_frame
	if partial_receiver.events.has("tile_loaded"):
		_fail("Canceled partial realization delivered tile_loaded")
		return

	# Separate terrain/object tilesets own separate request, worker, main-thread,
	# and cache budgets. Update a deliberately pressurized object hierarchy first
	# every frame; a small terrain fixture must still complete while object work
	# remains queued rather than borrowing or waiting for the object's allowance.
	var pressure_content_url := "file://" + ProjectSettings.globalize_path(
		"res://fixtures/lifecycle/triangle.gltf"
	)
	var pressure_url := _write_pressure_tileset(pressure_content_url, 24)
	if pressure_url.is_empty():
		_fail("Could not write the independent-budget pressure fixture")
		return
	var object_receiver := AdversarialReceiver.new()
	object_receiver.name = "ObjectPressureReceiver"
	test_root.add_child(object_receiver)
	var object_tileset := _create_tileset(
		georeference,
		object_receiver,
		pressure_url
	)
	object_tileset.http_cache_enabled = false
	object_tileset.worker_thread_count = 4
	object_tileset.maximum_simultaneous_tile_loads = 8
	object_tileset.main_thread_loading_time_limit_ms = 0.001
	object_tileset.maximum_cached_bytes = 4 * 1024 * 1024

	var terrain_receiver := AdversarialReceiver.new()
	terrain_receiver.name = "TerrainPriorityReceiver"
	test_root.add_child(terrain_receiver)
	var terrain_tileset := _create_tileset(
		georeference,
		terrain_receiver,
		fixture_url
	)
	terrain_tileset.http_cache_enabled = false
	terrain_tileset.worker_thread_count = 1
	terrain_tileset.maximum_simultaneous_tile_loads = 1
	terrain_tileset.main_thread_loading_time_limit_ms = 4.0
	terrain_tileset.maximum_cached_bytes = 2 * 1024 * 1024

	var terrain_loaded_under_pressure := false
	var object_still_pressurized := false
	for _frame in range(240):
		object_tileset.update_tileset(camera.global_transform)
		terrain_tileset.update_tileset(camera.global_transform)
		await process_frame
		if terrain_receiver.events.has("tile_loaded"):
			var object_statistics := object_tileset.get_streaming_statistics()
			object_still_pressurized = (
				int(object_statistics.worker_queue) > 0 or
				int(object_statistics.main_thread_queue) > 0 or
				float(object_statistics.load_progress_percent) < 100.0
			)
			terrain_loaded_under_pressure = true
			break
	var terrain_statistics := terrain_tileset.get_streaming_statistics()
	var object_statistics := object_tileset.get_streaming_statistics()
	if (
		not terrain_loaded_under_pressure or
		not object_still_pressurized or
		int(terrain_statistics.worker_threads) != 1 or
		int(object_statistics.worker_threads) != 4 or
		int(terrain_statistics.maximum_simultaneous_tile_loads) != 1 or
		int(object_statistics.maximum_simultaneous_tile_loads) != 8 or
		not is_equal_approx(float(terrain_statistics.main_thread_budget_ms), 4.0) or
		not is_equal_approx(float(object_statistics.main_thread_budget_ms), 0.001) or
		int(terrain_statistics.cache_limit_bytes) != 2 * 1024 * 1024 or
		int(object_statistics.cache_limit_bytes) != 4 * 1024 * 1024
	):
		_fail(
			"Terrain/object budgets were not independent: terrain=%s object=%s"
			% [terrain_statistics, object_statistics]
		)
		return
	object_tileset.free()
	object_tileset = null
	terrain_tileset.free()
	terrain_tileset = null
	await process_frame

	# Wait until the server confirms the content request is actively blocked,
	# then destroy the tileset. Completion after destruction must not dereference
	# the old Godot node or emit a late load callback.
	var server_url := OS.get_environment("CESIUM_TEST_SERVER_URL")
	var marker_path := OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	if server_url.is_empty() or marker_path.is_empty():
		_fail("Delayed fixture server environment was not configured")
		return

	# A cacheable HTTP tileset must survive a Native tileset generation and be
	# served from the documented SQLite cache on the next generation. This
	# catches accessors that discard Cache-Control/ETag response headers.
	var cache_token := "cache-%d" % Time.get_ticks_usec()
	var cache_url := server_url + "/cache-tileset.json?token=" + cache_token
	var first_cache_receiver := AdversarialReceiver.new()
	test_root.add_child(first_cache_receiver)
	var first_cache_tileset := _create_tileset(
		georeference,
		first_cache_receiver,
		cache_url
	)
	if not first_cache_tileset.clear_http_cache():
		_fail("Could not clear the deterministic HTTP cache before testing")
		return
	if not await _pump_target_until(
		first_cache_tileset,
		first_cache_receiver,
		"tile_loaded",
		0,
		FRAME_TIMEOUT
	):
		_fail("First cacheable HTTP tileset did not load")
		return
	var first_cache_statistics := first_cache_tileset.get_streaming_statistics()
	if (
		int(first_cache_statistics.http_cache_misses) < 2 or
		int(first_cache_statistics.http_cache_store_successes) < 2
	):
		_fail(
			"First HTTP load was not recorded as cache misses/stores: %s"
			% [first_cache_statistics]
		)
		return
	first_cache_tileset.free()
	first_cache_tileset = null
	await process_frame

	var second_cache_receiver := AdversarialReceiver.new()
	test_root.add_child(second_cache_receiver)
	var second_cache_tileset := _create_tileset(
		georeference,
		second_cache_receiver,
		cache_url
	)
	if not await _pump_target_until(
		second_cache_tileset,
		second_cache_receiver,
		"tile_loaded",
		0,
		FRAME_TIMEOUT
	):
		_fail("Cached HTTP tileset did not load on its second generation")
		return
	var second_cache_statistics := second_cache_tileset.get_streaming_statistics()
	if int(second_cache_statistics.http_cache_hits) < 2:
		_fail(
			"Second HTTP generation did not hit both cached responses: %s"
			% [second_cache_statistics]
		)
		return
	var marker_contents := FileAccess.get_file_as_string(marker_path)
	if (
		marker_contents.count("cache-tileset:" + cache_token) != 1 or
		marker_contents.count("cache-content:" + cache_token) != 1
	):
		_fail("Cache hits unexpectedly reached the local HTTP fixture again")
		return
	if not second_cache_tileset.prune_http_cache():
		_fail("Manual HTTP cache prune failed")
		return
	if not second_cache_tileset.clear_http_cache():
		_fail("Manual HTTP cache clear failed")
		return
	second_cache_statistics = second_cache_tileset.get_streaming_statistics()
	if (
		int(second_cache_statistics.http_cache_prunes) < 1 or
		int(second_cache_statistics.http_cache_clears) < 1
	):
		_fail("Manual cache maintenance was not observable in statistics")
		return
	second_cache_tileset.free()
	second_cache_tileset = null
	await process_frame

	# Turning away while a tile body is in flight must cancel the transport and
	# discard every later CPU/GPU stage without requiring tileset destruction.
	var stale_receiver := AdversarialReceiver.new()
	stale_receiver.name = "StaleRequestReceiver"
	test_root.add_child(stale_receiver)
	var stale_token := "stale-%d" % Time.get_ticks_usec()
	var stale_tileset := _create_tileset(
		georeference,
		stale_receiver,
		server_url + "/slow-tileset.json?token=" + stale_token
	)
	if not await _pump_until_marker(
		stale_tileset,
		marker_path,
		stale_token,
		2000
	):
		_fail("Stale-request content transfer did not start")
		return
	_move_camera_away()
	for _frame in range(8):
		stale_tileset.update_tileset(camera.global_transform)
		await process_frame
	var stale_statistics := stale_tileset.get_streaming_statistics()
	if (
		not bool(stale_statistics.stale_request_cancellation_enabled) or
		int(stale_statistics.canceled_tile_loads) < 1
	):
		_fail("Turning away did not cancel stale tile work: %s" % [stale_statistics])
		return
	var stale_deadline := Time.get_ticks_msec() + 1000
	while Time.get_ticks_msec() < stale_deadline:
		stale_tileset.update_tileset(camera.global_transform)
		await process_frame
	if stale_receiver.events.has("tile_loaded"):
		_fail("Canceled stale content was still realized")
		return
	stale_tileset.free()
	stale_tileset = null
	await process_frame
	_move_camera_home()

	var cancellation_receiver := AdversarialReceiver.new()
	cancellation_receiver.name = "CancellationReceiver"
	test_root.add_child(cancellation_receiver)
	var cancellation_token := "cancel-%d" % Time.get_ticks_usec()
	var cancellation_tileset := _create_tileset(
		georeference,
		cancellation_receiver,
		server_url + "/slow-tileset.json?token=" + cancellation_token
	)
	var cancellation_visualizer := CesiumTileDebugVisualizer.new()
	cancellation_visualizer.tileset = cancellation_tileset
	cancellation_visualizer.show_labels = false
	cancellation_visualizer.maximum_labels = 0
	test_root.add_child(cancellation_visualizer)
	if not await _pump_until_marker(
		cancellation_tileset,
		marker_path,
		cancellation_token,
		2000
	):
		_fail("Delayed content request did not start")
		return
	await process_frame
	if (
		cancellation_tileset.debug_tile_states.is_empty() or
		cancellation_visualizer.drawn_state_count < 1
	):
		_fail("Debug snapshots did not remain usable during an active request")
		return
	# Destroy the diagnostic consumer first, while the transport and Native tile
	# still exist. Its ObjectID and copied-state boundary must not participate in
	# renderer ownership or cancellation.
	cancellation_visualizer.free()
	cancellation_visualizer = null
	cancellation_tileset.free()
	cancellation_tileset = null
	await _wait_milliseconds(1000)
	if cancellation_receiver.events.has("tile_loaded"):
		_fail("Destroyed tileset delivered a late tile_loaded callback")
		return

	# Repeat while destroying the entire owning subtree, including the receiver
	# and georeference, to cover shutdown with requests active.
	var shutdown_root := Node3D.new()
	shutdown_root.name = "ShutdownSubtree"
	test_root.add_child(shutdown_root)
	var shutdown_georeference := _create_georeference(shutdown_root)
	var shutdown_receiver := AdversarialReceiver.new()
	shutdown_root.add_child(shutdown_receiver)
	var shutdown_token := "shutdown-%d" % Time.get_ticks_usec()
	var shutdown_tileset := _create_tileset(
		shutdown_georeference,
		shutdown_receiver,
		server_url + "/slow-tileset.json?token=" + shutdown_token
	)
	if not await _pump_until_marker(
		shutdown_tileset,
		marker_path,
		shutdown_token,
		2000
	):
		_fail("Shutdown content request did not start")
		return
	shutdown_root.free()
	shutdown_tileset = null
	shutdown_receiver = null
	shutdown_georeference = null
	await _wait_milliseconds(1000)

	print("Cesium adversarial lifecycle tests passed: ", receiver.events)
	test_root.free()
	for _frame in range(8):
		await process_frame
	quit(0)


func _create_camera(parent: Node) -> Camera3D:
	var result := Camera3D.new()
	result.current = true
	parent.add_child(result)
	result.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	result.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	return result


func _create_georeference(parent: Node) -> CesiumGeoreference:
	var result := CesiumGeoreference.new()
	result.origin_type = CesiumGeoreference.TrueOrigin
	parent.add_child(result)
	return result


func _create_tileset(
	parent: CesiumGeoreference,
	event_receiver: Cesium3DTilesetLifecycleEventReceiver,
	url: String
) -> Cesium3DTileset:
	var result := Cesium3DTileset.new()
	result.data_source = 1
	result.url = url
	result.create_physics_meshes = false
	result.maximum_screen_space_error = 1.0
	result.maximum_simultaneous_tile_loads = 2
	result.preload_ancestors = false
	result.preload_siblings = false
	result.http_cache_path = "user://cesium-tests/http-cache.sqlite"
	result.http_cache_maximum_items = 64
	result.http_cache_maximum_data_bytes = 16 * 1024 * 1024
	result.http_cache_prune_interval_requests = 4
	result.lifecycle_event_receiver = event_receiver
	parent.add_child(result)
	return result


func _pump_until(event_name: String, start_index: int, frame_limit: int) -> bool:
	for _frame in range(frame_limit):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.find(event_name, start_index) >= 0:
			return true
		await process_frame
	return false


func _pump_target_until(
	target_tileset: Cesium3DTileset,
	target_receiver: Cesium3DTilesetLifecycleEventReceiver,
	event_name: String,
	start_index: int,
	frame_limit: int
) -> bool:
	for _frame in range(frame_limit):
		target_tileset.update_tileset(camera.global_transform)
		if target_receiver.events.find(event_name, start_index) >= 0:
			return true
		await process_frame
	return false


func _pump_for_milliseconds(milliseconds: int) -> void:
	var deadline := Time.get_ticks_msec() + milliseconds
	while Time.get_ticks_msec() < deadline:
		if is_instance_valid(tileset):
			tileset.update_tileset(camera.global_transform)
		await process_frame


func _pump_until_marker(
	target_tileset: Cesium3DTileset,
	marker_path: String,
	token: String,
	timeout_milliseconds: int
) -> bool:
	var deadline := Time.get_ticks_msec() + timeout_milliseconds
	while Time.get_ticks_msec() < deadline:
		target_tileset.update_tileset(camera.global_transform)
		if FileAccess.file_exists(marker_path):
			var marker := FileAccess.get_file_as_string(marker_path)
			if marker.contains(token):
				return true
		await process_frame
	return false


func _wait_milliseconds(milliseconds: int) -> void:
	var deadline := Time.get_ticks_msec() + milliseconds
	while Time.get_ticks_msec() < deadline:
		await process_frame


func _move_camera_home() -> void:
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)


func _move_camera_away() -> void:
	camera.position = Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
	camera.look_at(Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0), Vector3.UP)


func _write_replacement_tileset() -> String:
	var content_path := ProjectSettings.globalize_path(
		"res://fixtures/lifecycle/triangle.gltf"
	)
	return _write_generated_tileset(
		"replacement.json",
		"file://" + content_path
	)


func _write_malformed_tileset() -> String:
	var output_directory := ProjectSettings.globalize_path("res://.test-generated")
	DirAccess.make_dir_recursive_absolute(output_directory)
	var broken_path := output_directory.path_join("broken.gltf")
	var broken := FileAccess.open(broken_path, FileAccess.WRITE)
	if broken == null:
		return ""
	broken.store_string("{ this is deliberately not glTF JSON")
	broken.close()
	return _write_generated_tileset("malformed.json", "file://" + broken_path)


func _write_pressure_tileset(content_uri: String, child_count: int) -> String:
	var output_directory := ProjectSettings.globalize_path("res://.test-generated")
	DirAccess.make_dir_recursive_absolute(output_directory)
	var children: Array[Dictionary] = []
	for _index in range(child_count):
		children.append({
			"boundingVolume": {
				"box": [
					50, 0, 50,
					100, 0, 0,
					0, 100, 0,
					0, 0, 100,
				]
			},
			"geometricError": 0,
			"content": {"uri": content_uri},
		})
	var definition := {
		"asset": {"version": "1.1"},
		"geometricError": 1000,
		"root": {
			"transform": [
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				EARTH_RADIUS, 0, 0, 1,
			],
			"boundingVolume": {
				"box": [
					50, 0, 50,
					100, 0, 0,
					0, 100, 0,
					0, 0, 100,
				]
			},
			"geometricError": 1000,
			"refine": "ADD",
			"children": children,
		},
	}
	var output_path := output_directory.path_join("pressure.json")
	var output := FileAccess.open(output_path, FileAccess.WRITE)
	if output == null:
		return ""
	output.store_string(JSON.stringify(definition, "\t"))
	output.close()
	return "file://" + output_path


func _write_generated_tileset(file_name: String, content_uri: String) -> String:
	var output_directory := ProjectSettings.globalize_path("res://.test-generated")
	DirAccess.make_dir_recursive_absolute(output_directory)
	var output_path := output_directory.path_join(file_name)
	var definition := {
		"asset": {"version": "1.1"},
		"geometricError": 0,
		"root": {
			"transform": [
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				EARTH_RADIUS, 0, 0, 1,
			],
			"boundingVolume": {
				"box": [
					50, 0, 50,
					100, 0, 0,
					0, 100, 0,
					0, 0, 100,
				]
			},
			"geometricError": 0,
			"content": {"uri": content_uri},
		},
	}
	var output := FileAccess.open(output_path, FileAccess.WRITE)
	if output == null:
		return ""
	output.store_string(JSON.stringify(definition, "\t"))
	output.close()
	return "file://" + output_path


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
