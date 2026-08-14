extends SceneTree

const RecordingReceiver := preload("res://recording_lifecycle_receiver.gd")
const LOAD_TIMEOUT_FRAMES := 600
const TRANSITION_TIMEOUT_FRAMES := 180
const EARTH_RADIUS := 6_378_137.0

var test_root: Node3D
var camera: Camera3D
var receiver: Cesium3DTilesetLifecycleEventReceiver
var tileset: Cesium3DTileset
var overlay_texture_a: ImageTexture
var overlay_texture_b: ImageTexture
var overlay_binding_b: CesiumRasterOverlayBinding
var retained_primitive: CesiumLoadedTilePrimitive
var retained_tile_bounds: CesiumBoundingVolume
var retained_content_bounds: CesiumBoundingVolume
var retained_viewer_request_bounds: CesiumBoundingVolume
var retained_debug_state: CesiumTileDebugState


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "LifecycleStreamingTest"
	root.add_child(test_root)

	receiver = RecordingReceiver.new()
	receiver.name = "RecordingLifecycleReceiver"
	test_root.add_child(receiver)

	camera = Camera3D.new()
	camera.name = "TestCamera"
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.name = "LocalCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	tileset = Cesium3DTileset.new()
	tileset.name = "LifecycleFixtureTileset"
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/lifecycle/tileset.json"
	)
	tileset.create_physics_meshes = false
	# Make the resumable renderer yield deterministically in this fixture. The
	# production default remains 5 ms and may realize several small surfaces.
	tileset.main_thread_loading_time_limit_ms = 0.001
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.debug_tile_state_capture_enabled = true
	tileset.debug_tile_state_limit = 8
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.http_cache_path = "user://cesium-tests/lifecycle-http-cache.sqlite"
	tileset.http_cache_maximum_items = 32
	tileset.http_cache_maximum_data_bytes = 8 * 1024 * 1024
	tileset.http_cache_prune_interval_requests = 8
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	if not await _pump_until("visibility:true", LOAD_TIMEOUT_FRAMES):
		_fail("Timed out waiting for the fixture tile to load and become visible")
		return

	if not _check_initial_order():
		return
	if not _check_primitive_context():
		return
	if not _check_bounds_queries():
		return
	if not await _check_debug_tile_states():
		return
	retained_primitive = (
		receiver.primitive_snapshot.get("loaded_tile") as Cesium3DTile
	).get_loaded_tile_primitive(0)
	if not _check_streaming_statistics():
		return
	if not _check_hardware_budgets():
		return
	if not await _check_movement_prediction():
		return
	if not _attach_test_overlays():
		return
	if not _check_overlay_bindings():
		return

	# Removing one overlay must not disturb another binding on the same surface.
	var loaded_tile := receiver.primitive_snapshot.get("loaded_tile") as Cesium3DTile
	if loaded_tile == null:
		_fail("Primitive snapshot lost its loaded tile before overlay validation")
		return
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	if not tileset.detach_raster_overlay(primitive, "OverlayA", overlay_texture_a):
		_fail("OverlayA did not detach through the generic material contract")
		return
	if not receiver.events.has("raster_detaching:OverlayA"):
		_fail("OverlayA detach callback was not delivered synchronously")
		return
	var remaining_bindings: Array = loaded_tile.get_raster_overlay_bindings(0)
	if remaining_bindings.size() != 1:
		_fail("Removing OverlayA did not preserve exactly one binding: %s" % [remaining_bindings])
		return
	if (remaining_bindings[0] as CesiumRasterOverlayBinding).overlay_key != "OverlayB":
		_fail("Removing OverlayA removed or replaced OverlayB")
		return
	var remaining_material := (
		(remaining_bindings[0] as CesiumRasterOverlayBinding).material
		as ShaderMaterial
	)
	if remaining_material == null:
		_fail("OverlayB lost the custom ShaderMaterial after OverlayA detached")
		return
	if remaining_material.get_shader_parameter("overlaya_texture") != null:
		_fail("OverlayA shader state survived its detach")
		return
	if remaining_material.get_shader_parameter("overlayb_texture") == null:
		_fail("OverlayB shader state was cleared while detaching OverlayA")
		return
	if not _check_overlay_replacement(primitive, loaded_tile):
		return

	var first_visible_index: int = receiver.events.find("visibility:true")
	camera.position = Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
	camera.look_at(Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0), Vector3.UP)
	if not await _pump_until("visibility:false", TRANSITION_TIMEOUT_FRAMES):
		_fail("Timed out waiting for the fixture tile to become hidden")
		return

	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	if not await _pump_until_after(
		"visibility:true",
		first_visible_index,
		TRANSITION_TIMEOUT_FRAMES
	):
		_fail("Timed out waiting for the hidden fixture tile to become visible again")
		return

	# Delete the assigned receiver without clearing the tileset property. The
	# native ObjectID must become harmless instead of becoming a dangling node
	# pointer. Hide the tile once while no receiver exists.
	var initial_events: Array[String] = receiver.events.duplicate()
	receiver.free()
	receiver = null
	camera.position = Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
	camera.look_at(Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0), Vector3.UP)
	await _pump_frames(TRANSITION_TIMEOUT_FRAMES)

	# A replacement receiver should observe future transitions and final unload,
	# but must not receive fake load callbacks for an already-realized tile.
	receiver = RecordingReceiver.new()
	receiver.name = "ReplacementLifecycleReceiver"
	test_root.add_child(receiver)
	tileset.lifecycle_event_receiver = receiver
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	if not await _pump_until("visibility:true", TRANSITION_TIMEOUT_FRAMES):
		_fail("Replacement receiver did not observe the next visibility transition")
		return
	if receiver.events != ["visibility:true"]:
		_fail(
			"Replacement receiver received historical or out-of-order callbacks: %s"
			% [receiver.events]
		)
		return

	# A diagnostic node may outlive the source it observes. It must resolve the
	# weak tileset identity to null and discard its copied drawing resources on
	# the following frame.
	var owner_loss_visualizer := CesiumTileDebugVisualizer.new()
	owner_loss_visualizer.tileset = tileset
	owner_loss_visualizer.show_labels = false
	owner_loss_visualizer.maximum_labels = 0
	test_root.add_child(owner_loss_visualizer)
	await process_frame
	if owner_loss_visualizer.drawn_state_count < 1:
		_fail("Owner-loss visualizer did not draw before source destruction")
		return

	tileset.free()
	tileset = null
	await process_frame
	if (
		owner_loss_visualizer.tileset != null or
		owner_loss_visualizer.drawn_state_count != 0 or
		owner_loss_visualizer.drawn_frame_number != 0
	):
		_fail("Diagnostic visualizer retained state after tileset destruction")
		return
	owner_loss_visualizer.free()

	if not receiver.events.has("tile_unloading"):
		_fail("Destroying the tileset did not produce tile_unloading")
		return
	if not receiver.unloading_tile_was_valid:
		_fail("tile_unloading ran after the Godot tile was already invalid")
		return
	if retained_primitive.loaded_tile != null:
		_fail("Retained primitive kept a dangling tile after unload")
		return
	if (
		retained_primitive.default_material != null or
		retained_primitive.selected_base_material != null or
		retained_primitive.active_material != null or
		not retained_primitive.raster_overlay_bindings.is_empty()
	):
		_fail("Retained primitive kept unloaded renderer resources alive")
		return
	var unloaded_diagnostics: Dictionary = retained_primitive.get_render_diagnostics()
	if (
		bool(unloaded_diagnostics.get("loaded_tile_available", true)) or
		bool(unloaded_diagnostics.get("render_node_available", true)) or
		str(unloaded_diagnostics.get("render_state", "")) != "unloaded" or
		str(unloaded_diagnostics.get("active_material_origin", "")) != "unavailable" or
		bool(
			(unloaded_diagnostics.get("default_material", {}) as Dictionary).get(
				"available",
				true
			)
		) or
		bool(
			(unloaded_diagnostics.get("active_material", {}) as Dictionary).get(
				"available",
				true
			)
		) or
		not (unloaded_diagnostics.get("raster_overlays", []) as Array).is_empty() or
		str(
			(unloaded_diagnostics.get("source_material", {}) as Dictionary).get(
				"name",
				""
			)
		) != "LifecycleTestMaterial"
	):
		_fail(
			"Retained primitive diagnostics were unsafe or ambiguous after unload: %s"
			% [unloaded_diagnostics]
		)
		return
	if (
		retained_debug_state == null or
		retained_debug_state.tile_id.is_empty() or
		retained_debug_state.tile_bounds == null or
		not retained_debug_state.tile_bounds.is_valid()
	):
		_fail("Retained tile debug state depended on its unloaded Native tile")
		return
	if (
		retained_tile_bounds == null or not retained_tile_bounds.is_valid() or
		retained_content_bounds == null or
		retained_content_bounds.volume_type_name != "sphere" or
		retained_viewer_request_bounds == null or
		retained_viewer_request_bounds.get_details().get("radius", 0.0) != 1000.0
	):
		_fail("Retained bounding-volume Resources became invalid after unload")
		return
	var retained_extras := retained_primitive.tile_extras as Dictionary
	if retained_extras.get("placement_id", "") != "chunk_n25_26:17:2":
		_fail("Retained primitive lost its copied tile extras after unload")
		return

	print(
		"Cesium lifecycle streaming test passed: initial=",
		initial_events,
		" replacement=",
		receiver.events
	)
	test_root.free()
	quit(0)


func _pump_until(event_name: String, frame_limit: int) -> bool:
	for _frame in range(frame_limit):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.has(event_name):
			return true
		await process_frame
	return false


func _pump_until_after(
	event_name: String,
	previous_index: int,
	frame_limit: int
) -> bool:
	for _frame in range(frame_limit):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.find(event_name, previous_index + 1) >= 0:
			return true
		await process_frame
	return false


func _pump_frames(frame_count: int) -> void:
	for _frame in range(frame_count):
		tileset.update_tileset(camera.global_transform)
		await process_frame


func _check_initial_order() -> bool:
	var expected: Array[String] = [
		"create_material",
		"customize_material",
		"primitive_loaded",
		"create_material",
		"customize_material",
		"primitive_loaded",
		"create_material",
		"customize_material",
		"primitive_loaded",
		"create_material",
		"customize_material",
		"primitive_loaded",
		"create_material",
		"customize_material",
		"primitive_loaded",
		"create_material",
		"customize_material",
		"primitive_loaded",
		"tile_loaded",
		"visibility:true",
	]
	if receiver.events.size() < expected.size():
		_fail("Lifecycle sequence was incomplete: %s" % [receiver.events])
		return false
	for index in range(expected.size()):
		if receiver.events[index] != expected[index]:
			_fail(
				"Lifecycle order differed at %d: expected %s, received %s"
				% [index, expected[index], receiver.events[index]]
			)
			return false
	return true


func _check_streaming_statistics() -> bool:
	if not is_zero_approx(float(tileset.tile_cache_unload_time_limit_ms)):
		_fail(
			"Cache eviction must drain by default so maximum_cached_bytes " +
			"remains enforceable"
		)
		return false
	tileset.main_thread_loading_time_limit_ms = 3.5
	tileset.tile_cache_unload_time_limit_ms = 2.5
	var statistics := tileset.get_streaming_statistics()
	for key in [
		"worker_queue",
		"main_thread_queue",
		"selected",
		"loaded",
		"realized",
		"visible",
		"hidden",
		"loaded_data_bytes",
		"maximum_simultaneous_tile_loads",
		"worker_preparation_count",
		"main_thread_realization_count",
		"main_thread_realization_step_count",
		"main_thread_realization_step_maximum_us",
		"incremental_realization_yields",
		"oversized_payload_rejections",
		"maximum_primitive_geometry_upload_bytes",
		"maximum_primitive_texture_upload_bytes",
		"prepared_geometry_maximum_tile_bytes",
		"prepared_texture_maximum_tile_bytes",
		"prepared_geometry_maximum_primitive_bytes",
		"prepared_texture_maximum_primitive_bytes",
		"http_cache_resolved_path",
		"http_cache_maximum_items",
		"http_cache_maximum_data_bytes",
		"http_cache_disk_bytes",
		"http_cache_lookups",
		"http_cache_hits",
		"http_cache_misses",
		"movement_prediction_enabled",
		"movement_prediction_active",
		"movement_prediction_speed",
		"movement_prediction_distance",
		"movement_prediction_translation_active",
		"turn_prediction_active",
		"turn_prediction_angle_degrees",
		"zoom_out_prediction_active",
		"zoom_out_prediction_projection_scale",
		"predicted_view_direction_components",
		"movement_prediction_worker_queue",
		"movement_prediction_main_thread_queue",
		"stale_request_cancellation_enabled",
		"canceled_tile_loads",
		"debug_tile_state_capture_enabled",
		"debug_tile_state_count",
		"debug_tile_state_frame",
		"debug_tile_states_truncated",
		"cache_budget_source",
		"automatic_hardware_budgets_enabled",
		"hardware_budget_profile",
		"hardware_budget_profile_name",
		"automatic_cache_budget_share",
		"hardware_capabilities",
		"worker_thread_count_source",
	]:
		if not statistics.has(key):
			_fail("Streaming statistics omitted %s: %s" % [key, statistics])
			return false
	if (
		int(statistics.worker_threads) < 1 or
		int(statistics.maximum_simultaneous_tile_loads) != 4 or
		int(statistics.worker_preparation_count) < 1 or
		int(statistics.main_thread_realization_count) < 1 or
		int(statistics.main_thread_realization_step_count) < 1 or
		int(statistics.incremental_realization_yields) < 1 or
		int(statistics.realized) != 1 or
		int(statistics.visible) != 1 or
		int(statistics.prepared_geometry_maximum_tile_bytes) <= 0 or
		int(statistics.prepared_texture_maximum_tile_bytes) <= 0 or
		int(statistics.prepared_geometry_maximum_primitive_bytes) <= 0 or
		int(statistics.prepared_texture_maximum_primitive_bytes) <= 0
	):
		_fail("Streaming statistics did not describe the realized fixture: %s" % [statistics])
		return false
	if (
		not is_equal_approx(float(statistics.main_thread_budget_ms), 3.5) or
		not is_equal_approx(float(statistics.cache_unload_budget_ms), 2.5)
	):
		_fail("Live main-thread/cache budgets were not reflected in statistics")
		return false
	if (
		int(statistics.maximum_primitive_geometry_upload_bytes) !=
			16 * 1024 * 1024 or
		int(statistics.maximum_primitive_texture_upload_bytes) !=
			16 * 1024 * 1024 or
		int(statistics.oversized_payload_rejections) != 0
	):
		_fail("Payload validation defaults/statistics were incorrect: %s" % [statistics])
		return false
	if (
		str(statistics.http_cache_path) !=
			"user://cesium-tests/lifecycle-http-cache.sqlite" or
		int(statistics.http_cache_maximum_items) != 32 or
		int(statistics.http_cache_maximum_data_bytes) != 8 * 1024 * 1024 or
		int(statistics.http_cache_prune_interval_requests) != 8 or
		int(statistics.http_cache_lookups) < 2 or
		int(statistics.http_cache_misses) < 2 or
		not str(statistics.http_cache_resolved_path).ends_with(
			"cesium-tests/lifecycle-http-cache.sqlite"
		)
	):
		_fail("HTTP cache controls/statistics did not match the fixture: %s" % [statistics])
		return false
	return true


func _check_hardware_budgets() -> bool:
	var cache_before_refresh := tileset.maximum_cached_bytes
	if tileset.recalculate_automatic_hardware_budgets() != cache_before_refresh:
		_fail("Capability refresh changed a non-automatic cache budget")
		return false
	var capabilities := tileset.get_hardware_capabilities()
	for key in [
		"logical_processor_count",
		"physical_memory_bytes",
		"available_memory_bytes",
		"rendering_device_available",
		"rendering_method",
		"rendering_driver",
		"video_adapter_name",
		"video_adapter_vendor",
		"video_adapter_type",
		"device_total_memory_bytes",
		"device_used_memory_bytes",
		"device_total_memory_available",
		"recommended_cache_bytes_conservative",
		"recommended_cache_bytes_balanced",
		"recommended_cache_bytes_aggressive",
	]:
		if not capabilities.has(key):
			_fail("Hardware capability snapshot omitted %s: %s" % [key, capabilities])
			return false
	if int(capabilities.logical_processor_count) < 1:
		_fail("Hardware capability snapshot reported no logical processor")
		return false
	for capacity_key in [
		"physical_memory_bytes",
		"available_memory_bytes",
		"device_total_memory_bytes",
		"device_used_memory_bytes",
	]:
		var capacity := int(capabilities.get(capacity_key, -1))
		if capacity != -1 and capacity <= 0:
			_fail("Hardware capability %s was neither known nor unavailable: %s" % [
				capacity_key,
				capabilities,
			])
			return false
	if (
		bool(capabilities.device_total_memory_available) !=
		(int(capabilities.device_total_memory_bytes) > 0)
	):
		_fail("Device-memory availability flag disagreed with its capacity")
		return false
	var rendering_device := RenderingServer.get_rendering_device()
	if bool(capabilities.rendering_device_available) != (rendering_device != null):
		_fail("Hardware snapshot disagreed with RenderingServer device availability")
		return false
	if rendering_device != null:
		var used_device_memory := int(
			rendering_device.get_memory_usage(RenderingDevice.MEMORY_TOTAL)
		)
		var expected_used_device_memory := (
			used_device_memory if used_device_memory > 0 else -1
		)
		if int(capabilities.device_used_memory_bytes) != expected_used_device_memory:
			_fail("Hardware snapshot disagreed with RenderingDevice memory usage")
			return false
		if rendering_device.has_method("get_device_total_memory"):
			var raw_total_device_memory := int(
				rendering_device.call("get_device_total_memory")
			)
			var expected_total_device_memory := (
				raw_total_device_memory if raw_total_device_memory > 0 else -1
			)
			if (
				int(capabilities.device_total_memory_bytes) !=
				expected_total_device_memory
			):
				_fail("Hardware snapshot disagreed with newer Godot VRAM capacity")
				return false

	var conservative := tileset.get_recommended_total_cache_bytes(
		Cesium3DTileset.HardwareBudgetConservative
	)
	var balanced := tileset.get_recommended_total_cache_bytes(
		Cesium3DTileset.HardwareBudgetBalanced
	)
	var aggressive := tileset.get_recommended_total_cache_bytes(
		Cesium3DTileset.HardwareBudgetAggressive
	)
	if (
		conservative <= 0 or balanced < conservative or aggressive < balanced or
		conservative > 2 * 1024 * 1024 * 1024 or
		balanced > 8 * 1024 * 1024 * 1024 or
		aggressive > 16 * 1024 * 1024 * 1024 or
		conservative != int(capabilities.recommended_cache_bytes_conservative) or
		balanced != int(capabilities.recommended_cache_bytes_balanced) or
		aggressive != int(capabilities.recommended_cache_bytes_aggressive)
	):
		_fail("Hardware cache recommendations were inconsistent: %s" % [capabilities])
		return false

	# Exercise precedence on a detached node so changing its worker executor does
	# not disturb the streamed lifecycle fixture above.
	var budget_tileset := Cesium3DTileset.new()
	budget_tileset.get_hardware_capabilities()
	if budget_tileset.automatic_hardware_budgets_enabled:
		budget_tileset.free()
		_fail("Automatic hardware budgets unexpectedly changed the compatibility default")
		return false
	budget_tileset.maximum_cached_bytes = 123_456_789
	var budget_statistics := budget_tileset.get_streaming_statistics()
	if (
		budget_tileset.automatic_hardware_budgets_enabled or
		str(budget_statistics.cache_budget_source) != "explicit"
	):
		budget_tileset.free()
		_fail("An explicit cache budget did not remain authoritative")
		return false
	budget_tileset.hardware_budget_profile = Cesium3DTileset.HardwareBudgetBalanced
	budget_tileset.automatic_cache_budget_share = 0.25
	budget_tileset.automatic_hardware_budgets_enabled = true
	var expected_share := int(
		float(budget_tileset.get_recommended_total_cache_bytes(
			Cesium3DTileset.HardwareBudgetBalanced
		)) * 0.25
	)
	budget_statistics = budget_tileset.get_streaming_statistics()
	if (
		budget_tileset.maximum_cached_bytes != expected_share or
		str(budget_statistics.cache_budget_source) != "automatic_hardware"
	):
		budget_tileset.free()
		_fail("Opt-in automatic cache sizing did not apply the tileset share")
		return false
	budget_tileset.maximum_cached_bytes = 234_567_890
	budget_statistics = budget_tileset.get_streaming_statistics()
	if (
		budget_tileset.automatic_hardware_budgets_enabled or
		str(budget_statistics.cache_budget_source) != "explicit"
	):
		budget_tileset.free()
		_fail("An explicit cache override did not disable automatic sizing")
		return false
	budget_tileset.automatic_hardware_budgets_enabled = true
	budget_tileset.automatic_hardware_budgets_enabled = false
	budget_statistics = budget_tileset.get_streaming_statistics()
	if str(budget_statistics.cache_budget_source) != "automatic_frozen":
		budget_tileset.free()
		_fail("Disabling automatic sizing did not preserve and label the frozen value")
		return false

	budget_tileset.worker_thread_count = 3
	budget_statistics = budget_tileset.get_streaming_statistics()
	if str(budget_statistics.worker_thread_count_source) != "explicit":
		budget_tileset.free()
		_fail("Explicit worker count was not labeled as explicit")
		return false
	budget_tileset.reset_worker_thread_count_to_automatic()
	budget_statistics = budget_tileset.get_streaming_statistics()
	if (
		str(budget_statistics.worker_thread_count_source) != "automatic_cpu" or
		int(budget_statistics.worker_threads) < 1 or
		int(budget_statistics.worker_threads) > 8
	):
		budget_tileset.free()
		_fail("CPU-derived worker count was not bounded or observable")
		return false
	budget_tileset.free()
	return true


func _check_debug_tile_states() -> bool:
	var states: Array = tileset.debug_tile_states
	if states.size() < 2:
		_fail("Debug capture omitted the selected tile or its hierarchy: %s" % [states])
		return false
	var selected: CesiumTileDebugState
	for value in states:
		var state := value as CesiumTileDebugState
		if state == null:
			_fail("Debug capture returned a non-CesiumTileDebugState value")
			return false
		if state.selection_role == CesiumTileDebugState.ROLE_SELECTED:
			selected = state
	if (
		selected == null or selected.load_state != CesiumTileDebugState.LOAD_DONE or
		selected.refine_mode_name != "replace" or selected.geometric_error != 0.0 or
		selected.parent_hierarchy_path.is_empty() or selected.tile_bounds == null or
		selected.content_bounds == null or selected.viewer_request_bounds == null or
		absf(selected.world_bounds_transform.basis.determinant()) < 0.001
	):
		_fail("Selected debug state was incomplete: %s" % [
			selected.to_dictionary() if selected != null else null
		])
		return false
	var parent_found := false
	for value in states:
		var state := value as CesiumTileDebugState
		if state.hierarchy_path == selected.parent_hierarchy_path:
			parent_found = true
			break
	if not parent_found:
		_fail("Debug hierarchy did not connect the selected child to its ancestor")
		return false
	retained_debug_state = selected

	# The returned Array is a copy; callers cannot mutate the tileset's snapshot.
	states.clear()
	if tileset.debug_tile_states.size() < 2:
		_fail("Mutating a returned debug Array changed the tileset snapshot")
		return false

	# The built-in visualizer consumes only snapshots. Constructing, drawing, and
	# destroying it must be safe while the tileset continues streaming.
	var visualizer := CesiumTileDebugVisualizer.new()
	visualizer.tileset = tileset
	visualizer.show_labels = false
	visualizer.maximum_labels = 0
	test_root.add_child(visualizer)
	tileset.update_tileset(camera.global_transform)
	await process_frame
	await process_frame
	if (
		visualizer.drawn_state_count < 2 or
		visualizer.drawn_frame_number != tileset.debug_tile_state_frame_number
	):
		_fail("Tile debug visualizer did not draw the current snapshot")
		return false
	visualizer.free()
	await process_frame

	tileset.debug_tile_state_limit = 1
	tileset.update_tileset(camera.global_transform)
	if tileset.debug_tile_states.size() != 1 or not tileset.debug_tile_states_truncated:
		_fail("Debug snapshot limit did not bound hierarchy capture")
		return false
	tileset.debug_tile_state_limit = 8
	tileset.update_tileset(camera.global_transform)
	return true


func _check_bounds_queries() -> bool:
	var tileset_bounds := tileset.get_tileset_bounds() as CesiumBoundingVolume
	if (
		tileset_bounds == null or not tileset_bounds.is_valid() or
		tileset_bounds.volume_type != CesiumBoundingVolume.OrientedBox or
		tileset_bounds.volume_type_name != "oriented_box"
	):
		_fail("Tileset root bounding volume was unavailable or had the wrong type")
		return false
	var root_center: PackedFloat64Array = tileset_bounds.center_components
	if root_center.size() != 3 or absf(root_center[0] - 6_378_187.0) > 0.001:
		_fail("Root bounds did not preserve its transformed full-precision center")
		return false
	if tileset.get_tileset_source_aabb() != tileset_bounds.source_aabb:
		_fail("Tileset source AABB convenience query disagreed with its Resource")
		return false
	if tileset.get_tileset_bounds() != tileset_bounds:
		_fail("Repeated root-bounds queries did not reuse a stable Resource")
		return false

	var loaded_tile := (
		receiver.primitive_snapshot.get("loaded_tile") as Cesium3DTile
	)
	if loaded_tile == null:
		_fail("Bounds test lost its loaded tile")
		return false
	retained_tile_bounds = loaded_tile.tile_bounds
	retained_content_bounds = loaded_tile.content_bounds
	retained_viewer_request_bounds = loaded_tile.viewer_request_bounds
	if (
		retained_tile_bounds == null or
		retained_tile_bounds.volume_type != CesiumBoundingVolume.OrientedBox or
		retained_content_bounds == null or
		retained_content_bounds.volume_type != CesiumBoundingVolume.Sphere or
		retained_viewer_request_bounds == null or
		retained_viewer_request_bounds.volume_type != CesiumBoundingVolume.Sphere
	):
		_fail("Tile/content/viewer-request bounds were not exposed independently")
		return false
	if retained_content_bounds.get_details().get("radius", 0.0) != 75.0:
		_fail("Content bounding sphere radius was not preserved")
		return false
	if retained_viewer_request_bounds.get_details().get("radius", 0.0) != 1000.0:
		_fail("Viewer request bounding sphere radius was not preserved")
		return false
	if loaded_tile.get_tile_source_aabb() != retained_tile_bounds.source_aabb:
		_fail("Tile source AABB convenience query disagreed with its Resource")
		return false
	if not retained_tile_bounds.contains_source_position(
		retained_tile_bounds.center
	):
		_fail("Bounding volume did not contain its own center")
		return false
	if not retained_tile_bounds.contains_source_position_components(
		retained_tile_bounds.center_components
	):
		_fail("Full-precision bounding volume query lost its own center")
		return false
	if retained_tile_bounds.distance_to_source_position(
		retained_tile_bounds.center + Vector3(1000.0, 1000.0, 1000.0)
	) <= 0.0:
		_fail("Bounding volume distance query did not reject a distant position")
		return false
	if retained_tile_bounds.distance_to_source_position_components(
		PackedFloat64Array([99_000_000.0, 99_000_000.0, 99_000_000.0])
	) <= 0.0:
		_fail("Full-precision bounding volume distance query was invalid")
		return false
	var source_aabb: AABB = retained_content_bounds.source_aabb
	var shifted_aabb := retained_content_bounds.get_transformed_aabb(
		Transform3D(Basis.IDENTITY, Vector3(10.0, 20.0, 30.0))
	)
	if not shifted_aabb.position.is_equal_approx(
		source_aabb.position + Vector3(10.0, 20.0, 30.0)
	):
		_fail("Explicit transformed AABB did not apply its source transform")
		return false
	var mutable_details := retained_content_bounds.get_details()
	mutable_details["radius"] = -1.0
	if retained_content_bounds.get_details().get("radius", 0.0) != 75.0:
		_fail("Bounding volume details leaked mutable internal state")
		return false
	return true


func _check_movement_prediction() -> bool:
	tileset.movement_prediction_enabled = true
	tileset.movement_prediction_seconds = 2.0
	tileset.movement_prediction_minimum_speed = 0.0
	tileset.movement_prediction_maximum_distance = 25.0
	tileset.movement_prediction_weight = 0.2
	tileset.turn_prediction_enabled = true
	tileset.zoom_out_prediction_enabled = true

	var original_transform := camera.global_transform
	await process_frame
	camera.global_position += Vector3(50.0, 0.0, 0.0)
	tileset.update_tileset(camera.global_transform)
	var statistics := tileset.get_streaming_statistics()
	if (
		not bool(statistics.movement_prediction_enabled) or
		not bool(statistics.movement_prediction_active) or
		not bool(statistics.movement_prediction_translation_active) or
		float(statistics.movement_prediction_speed) <= 0.0 or
		float(statistics.movement_prediction_distance) <= 0.0 or
		float(statistics.movement_prediction_distance) > 25.001 or
		not is_equal_approx(float(statistics.movement_prediction_weight), 0.2)
	):
		_fail("Movement prediction did not create a bounded future view: %s" % [statistics])
		return false

	# A turn with translation suppressed should rotate the future selection view,
	# while respecting the authored maximum prediction angle.
	tileset.movement_prediction_enabled = false
	tileset.movement_prediction_minimum_speed = 1_000_000.0
	tileset.turn_prediction_minimum_angular_speed_degrees = 0.0
	tileset.turn_prediction_maximum_angle_degrees = 30.0
	tileset.zoom_out_prediction_enabled = false
	tileset.movement_prediction_enabled = true
	tileset.update_tileset(camera.global_transform)
	var before_turn_statistics := tileset.get_streaming_statistics()
	var before_turn_direction_components: PackedFloat64Array = \
		before_turn_statistics.view_direction_components
	await process_frame
	camera.rotate_y(deg_to_rad(45.0))
	tileset.update_tileset(camera.global_transform)
	statistics = tileset.get_streaming_statistics()
	var predicted_direction: PackedFloat64Array = \
		statistics.predicted_view_direction_components
	var visible_direction: PackedFloat64Array = statistics.view_direction_components
	var before_turn_direction := Vector3(
		before_turn_direction_components[0],
		before_turn_direction_components[1],
		before_turn_direction_components[2]
	)
	var current_direction := Vector3(
		visible_direction[0],
		visible_direction[1],
		visible_direction[2]
	)
	var future_direction := Vector3(
		predicted_direction[0],
		predicted_direction[1],
		predicted_direction[2]
	)
	if (
		not bool(statistics.movement_prediction_active) or
		bool(statistics.movement_prediction_translation_active) or
		not bool(statistics.turn_prediction_active) or
		float(statistics.turn_prediction_angular_speed_degrees) <= 0.0 or
		float(statistics.turn_prediction_angle_degrees) <= 0.0 or
		float(statistics.turn_prediction_angle_degrees) > 30.001 or
		predicted_direction.size() != 3 or visible_direction.size() != 3 or
		(future_direction - current_direction).length() <= 0.001 or
		before_turn_direction.cross(current_direction).dot(
			current_direction.cross(future_direction)
		) <= 0.0
	):
		_fail("Turn prediction did not create a bounded future direction: %s" % [statistics])
		return false

	# Zooming out expands the future projection, rather than changing the visible
	# projection or pretending that the camera translated.
	tileset.movement_prediction_enabled = false
	camera.global_transform = original_transform
	var original_fov := camera.fov
	camera.fov = 45.0
	tileset.turn_prediction_enabled = false
	tileset.zoom_out_prediction_enabled = true
	tileset.zoom_out_prediction_minimum_rate = 0.0
	tileset.zoom_out_prediction_maximum_scale = 1.5
	tileset.movement_prediction_enabled = true
	tileset.update_tileset(camera.global_transform)
	await process_frame
	camera.fov = 90.0
	tileset.update_tileset(camera.global_transform)
	statistics = tileset.get_streaming_statistics()
	if (
		not bool(statistics.movement_prediction_active) or
		bool(statistics.movement_prediction_translation_active) or
		bool(statistics.turn_prediction_active) or
		not bool(statistics.zoom_out_prediction_active) or
		float(statistics.zoom_out_prediction_rate) <= 0.0 or
		float(statistics.zoom_out_prediction_projection_scale) <= 1.0 or
		float(statistics.zoom_out_prediction_projection_scale) > 1.5001
	):
		_fail("Zoom-out prediction did not create a bounded wider future view: %s" % [statistics])
		return false
	camera.fov = original_fov

	tileset.movement_prediction_enabled = false
	tileset.update_tileset(camera.global_transform)
	statistics = tileset.get_streaming_statistics()
	if (
		bool(statistics.movement_prediction_active) or
		bool(statistics.movement_prediction_translation_active) or
		bool(statistics.turn_prediction_active) or
		bool(statistics.zoom_out_prediction_active) or
		int(statistics.movement_prediction_worker_queue) != 0 or
		int(statistics.movement_prediction_main_thread_queue) != 0
	):
		_fail("Disabling movement prediction left stale scheduled work: %s" % [statistics])
		return false

	camera.global_transform = original_transform
	tileset.movement_prediction_minimum_speed = 1.0
	tileset.turn_prediction_enabled = true
	tileset.turn_prediction_minimum_angular_speed_degrees = 5.0
	tileset.turn_prediction_maximum_angle_degrees = 90.0
	tileset.zoom_out_prediction_enabled = true
	tileset.zoom_out_prediction_minimum_rate = 0.1
	tileset.zoom_out_prediction_maximum_scale = 2.0
	tileset.movement_prediction_enabled = true
	return true


func _check_primitive_context() -> bool:
	var snapshot: Dictionary = receiver.primitive_snapshot
	if int(snapshot.get("surface_index", -1)) != 0:
		_fail("Expected fixture primitive to map to Godot surface 0")
		return false
	if int(snapshot.get("node_index", -1)) != 1:
		_fail("Expected fixture surface 0 to come from nested glTF node 1")
		return false
	if int(snapshot.get("mesh_index", -1)) != 0:
		_fail("Expected fixture primitive to come from glTF mesh 0")
		return false
	if int(snapshot.get("primitive_index", -1)) != 0:
		_fail("Expected fixture primitive index 0")
		return false
	if int(snapshot.get("material_index", -1)) != 0:
		_fail("Expected fixture glTF material index 0")
		return false
	if int(snapshot.get("primitive_mode", -1)) != 4:
		_fail("Expected fixture primitive mode TRIANGLES (4)")
		return false
	if not bool(snapshot.get("tile_valid", false)):
		_fail("Primitive context did not expose a valid tile during creation")
		return false
	var tile_extras := snapshot.get("tile_extras", {}) as Dictionary
	if tile_extras.get("placement_id", "") != "chunk_n25_26:17:2":
		_fail("Primitive context did not preserve tile placement identity")
		return false
	if tile_extras.get("lod_meshes", []) != ["high.gltf", "low.gltf"]:
		_fail("Primitive context did not preserve tile extras arrays")
		return false
	var source_extras := tile_extras.get("source", {}) as Dictionary
	if source_extras.get("class", "") != "CompoundObject":
		_fail("Primitive context did not preserve nested tile extras")
		return false
	var metadata_tile := snapshot.get("loaded_tile") as Cesium3DTile
	var direct_extras := metadata_tile.tile_extras as Dictionary
	if direct_extras.get("selection_distance", 0.0) != 12500.5:
		_fail("Loaded tile did not expose numeric tile extras")
		return false
	# Script callers receive deep copies. Mutating one snapshot must not alter
	# the tile or a subsequent primitive query.
	tile_extras["placement_id"] = "mutated"
	if (
		(metadata_tile.tile_extras as Dictionary).get("placement_id", "") !=
		"chunk_n25_26:17:2"
	):
		_fail("Tile extras were mutable through a returned Dictionary")
		return false
	if (
		(metadata_tile.get_loaded_tile_primitive(0).tile_extras as Dictionary).get(
			"placement_id", ""
		) != "chunk_n25_26:17:2"
	):
		_fail("Primitive tile extras were mutable through a returned Dictionary")
		return false
	var attributes: Dictionary = snapshot.get("attributes", {})
	if int(attributes.get("POSITION", -1)) != 0:
		_fail("Primitive context did not preserve the POSITION accessor")
		return false
	if not attributes.has("_CESIUMOVERLAY_0"):
		_fail("Primitive context did not preserve the raster overlay accessor: %s" % [snapshot])
		return false
	if int(snapshot.get("overlay_uv_index", -1)) != 0:
		_fail("Primitive context did not map _CESIUMOVERLAY_0 to Godot UV")
		return false
	var material: Dictionary = snapshot.get("gltf_material", {})
	if str(material.get("name", "")) != "LifecycleTestMaterial":
		_fail("Primitive context did not preserve the glTF material name")
		return false
	if not bool(snapshot.get("default_material_is_shader", false)):
		_fail("Default glTF material was not realized as a ShaderMaterial")
		return false
	var application_textures := snapshot.get(
		"default_application_textures", {}
	) as Dictionary
	if application_textures.size() != 1 or not application_textures.has(0):
		_fail(
			"Application texture indices were not validated and realized once: %s"
			% [application_textures]
		)
		return false
	if not application_textures[0] is Texture2D:
		_fail("Application texture index 0 did not expose a Texture2D")
		return false
	var shader_code := str(snapshot.get("default_shader_code", ""))
	for expected_shader_term in [
		"cull_disabled",
		"ALPHA_SCISSOR_THRESHOLD",
		"metallic_roughness_texture",
		"NORMAL_MAP",
		"AO = mix",
		"EMISSION",
	]:
		if not shader_code.contains(expected_shader_term):
			_fail("Default glTF shader omitted %s" % expected_shader_term)
			return false
	# LOD transitions are opt-in because their stochastic discard has a fragment
	# cost. The ordinary generated-material path must remain completely free of
	# transition uniforms and branches.
	if shader_code.contains("cesium_lod_transition_percentage"):
		_fail("Transition-disabled glTF shader retained the LOD transition path")
		return false
	if not (snapshot.get("default_base_color_factor") as Color).is_equal_approx(
		Color(0.1, 0.6, 0.9, 0.7)
	):
		_fail("glTF base-color factor was not preserved in linear space")
		return false
	if not is_equal_approx(float(snapshot.get("default_metallic_factor")), 0.25):
		_fail("glTF metallic factor was not preserved")
		return false
	if not is_equal_approx(float(snapshot.get("default_roughness_factor")), 0.75):
		_fail("glTF roughness factor was not preserved")
		return false
	if not (snapshot.get("default_emissive_factor") as Color).is_equal_approx(
		Color(0.2, 0.3, 0.4, 1.0)
	):
		_fail("glTF emissive factor was not preserved")
		return false
	if not is_equal_approx(float(snapshot.get("default_normal_scale")), 0.4):
		_fail("glTF normal scale was not preserved")
		return false
	if not is_equal_approx(float(snapshot.get("default_occlusion_strength")), 0.6):
		_fail("glTF occlusion strength was not preserved")
		return false
	if not is_equal_approx(float(snapshot.get("default_alpha_cutoff")), 0.33):
		_fail("glTF alpha cutoff was not preserved")
		return false
	for texture_key in [
		"default_base_texture",
		"default_metallic_roughness_texture",
		"default_normal_texture",
		"default_occlusion_texture",
		"default_emissive_texture",
	]:
		if snapshot.get(texture_key) == null:
			_fail("glTF material did not retain decoded %s" % texture_key)
			return false
	var shared_texture: Texture2D = snapshot.get("default_base_texture")
	for shared_texture_key in [
		"default_metallic_roughness_texture",
		"default_normal_texture",
		"default_occlusion_texture",
		"default_emissive_texture",
	]:
		if snapshot.get(shared_texture_key) != shared_texture:
			_fail("One decoded glTF image was uploaded more than once per model")
			return false
	if int(snapshot.get("default_base_uv_set", -1)) != 1:
		_fail(
			"KHR_texture_transform did not override the base-color UV set: %s"
			% [snapshot.get("default_base_uv_set")]
		)
		return false
	if not (snapshot.get("default_base_transform") as Vector4).is_equal_approx(
		Vector4(0.25, 0.5, 0.5, 0.75)
	):
		_fail("KHR_texture_transform offset/scale was not preserved")
		return false
	if not is_equal_approx(float(snapshot.get("default_base_rotation")), 0.125):
		_fail("KHR_texture_transform rotation was not preserved")
		return false
	if snapshot.get("default_base_wrap") != Vector2i(2, 0):
		_fail("glTF mirrored/clamped sampler wrap was not preserved")
		return false
	if bool(snapshot.get("default_sampler_exact", true)):
		_fail("Mixed glTF min/mag filters were incorrectly reported as exact")
		return false
	if receiver.primitive_snapshots.size() != 6:
		_fail(
			"Repeated three-primitive glTF mesh did not produce six instances: %s"
			% [receiver.primitive_snapshots]
		)
		return false
	var unlit_snapshot: Dictionary = receiver.primitive_snapshots[1]
	if (
		int(unlit_snapshot.get("material_index", -1)) != 1 or
		not str(unlit_snapshot.get("default_shader_code", "")).contains("unshaded")
	):
		_fail("KHR_materials_unlit was not preserved on the second primitive")
		return false
	var missing_material_snapshot: Dictionary = receiver.primitive_snapshots[2]
	if (
		int(missing_material_snapshot.get("material_index", 0)) != -1 or
		str(missing_material_snapshot.get("default_shader_code", "")).is_empty() or
		bool(
			(missing_material_snapshot.get("gltf_material", {}) as Dictionary).get(
				"defined",
				true
			)
		)
	):
		_fail("Primitive without a glTF material did not receive the spec default")
		return false
	var second_snapshot: Dictionary = receiver.primitive_snapshots[3]
	if (
		int(second_snapshot.get("surface_index", -1)) != 3 or
		int(second_snapshot.get("node_index", -1)) != 2 or
		int(second_snapshot.get("mesh_index", -1)) != 0 or
		int(second_snapshot.get("primitive_index", -1)) != 0
	):
		_fail("Repeated glTF mesh context lost its source node/mesh identity")
		return false
	if (
		int(second_snapshot.get("default_material_id", 0)) !=
		int(snapshot.get("default_material_id", -1))
	):
		_fail("Repeated glTF mesh instances did not share their immutable material")
		return false
	var loaded_tile := snapshot.get("loaded_tile") as Cesium3DTile
	if loaded_tile == null or loaded_tile.mesh.get_surface_count() != 6:
		_fail("Repeated glTF mesh nodes did not realize all six Godot surfaces")
		return false
	var first_vertices: PackedVector3Array = loaded_tile.mesh.surface_get_arrays(0)[
		Mesh.ARRAY_VERTEX
	]
	var first_normals: PackedVector3Array = loaded_tile.mesh.surface_get_arrays(0)[
		Mesh.ARRAY_NORMAL
	]
	var first_tangents: PackedFloat32Array = loaded_tile.mesh.surface_get_arrays(0)[
		Mesh.ARRAY_TANGENT
	]
	var second_vertices: PackedVector3Array = loaded_tile.mesh.surface_get_arrays(3)[
		Mesh.ARRAY_VERTEX
	]
	if first_normals.size() != 3 or first_tangents.size() != 12:
		_fail("Lit normal-mapped glTF primitive did not generate normals/tangents")
		return false
	for tangent_index in range(0, first_tangents.size(), 4):
		var tangent := Vector3(
			first_tangents[tangent_index],
			first_tangents[tangent_index + 1],
			first_tangents[tangent_index + 2]
		)
		if (
			not is_equal_approx(tangent.length(), 1.0) or
			not is_equal_approx(abs(first_tangents[tangent_index + 3]), 1.0)
		):
			_fail("MikkTSpace produced a non-unit tangent frame")
			return false
	if not is_equal_approx(first_vertices[0].x, 15.0):
		_fail("Nested glTF translation was not flattened into surface 0")
		return false
	if (
		not is_equal_approx(second_vertices[0].x, 30.0) or
		not is_equal_approx(
			min(second_vertices[0].x, second_vertices[1].x, second_vertices[2].x),
			-70.0
		)
	):
		_fail(
			"Nested negative-scale glTF transform was not applied: %s"
			% [second_vertices]
		)
		return false
	var absolute_origin := snapshot.get("absolute_origin", Vector3.ZERO) as Vector3
	var origin_high := snapshot.get("shader_origin_high", Vector3.ZERO) as Vector3
	var origin_low := snapshot.get("shader_origin_low", Vector3.ZERO) as Vector3
	if not (origin_high + origin_low).is_equal_approx(absolute_origin):
		_fail("Split shader origin does not reconstruct the stable absolute origin")
		return false
	if not is_equal_approx(absolute_origin.x, EARTH_RADIUS):
		_fail("Primitive absolute origin did not preserve the tileset transform")
		return false
	var shader_basis := snapshot.get("shader_basis", Basis()) as Basis
	if not shader_basis.is_equal_approx(
		snapshot.get("local_to_absolute_basis", Basis()) as Basis
	):
		_fail("Shader did not receive the primitive's local-to-absolute basis")
		return false
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	var diagnostics: Dictionary = primitive.get_render_diagnostics()
	if (
		primitive.default_material == null or
		primitive.selected_base_material == null or
		primitive.active_material != primitive.selected_base_material or
		not primitive.uses_custom_material or
		str(diagnostics.get("active_material_origin", "")) !=
			"lifecycle_custom_material" or
		bool(diagnostics.get("fallback_active", true)) or
		str(
			(diagnostics.get("default_material", {}) as Dictionary).get(
				"material_source",
				""
			)
		) != "gltf"
	):
		_fail("Primitive material selection diagnostics were incomplete: %s" % [diagnostics])
		return false
	var base_color_slot: Dictionary
	for value in diagnostics.get("texture_slots", []):
		var slot := value as Dictionary
		if slot.get("role", "") == "base_color":
			base_color_slot = slot
			break
	if (
		base_color_slot.is_empty() or
		int(base_color_slot.get("source_uv_set", -1)) != 1 or
		int(base_color_slot.get("godot_uv_index", -1)) != 1 or
		int(base_color_slot.get("generated_shader_uv_set", -1)) != 1 or
		not bool(
			(base_color_slot.get("generated_texture", {}) as Dictionary).get(
				"available",
				false
			)
		)
	):
		_fail("Texture-slot diagnostics lost the effective KHR UV set: %s" % [base_color_slot])
		return false
	var missing_primitive := loaded_tile.get_loaded_tile_primitive(2)
	var missing_diagnostics: Dictionary = missing_primitive.get_render_diagnostics()
	if (
		not bool(missing_diagnostics.get("fallback_active", false)) or
		str(missing_diagnostics.get("fallback_reason", "")) !=
			"missing_gltf_material"
	):
		_fail("Missing-material fallback was not diagnosable: %s" % [missing_diagnostics])
		return false
	return true


func _check_overlay_bindings() -> bool:
	var snapshots: Array[Dictionary] = receiver.overlay_snapshots
	var attached: Array[Dictionary] = snapshots.filter(
		func(snapshot: Dictionary) -> bool:
			return snapshot.get("event") == "attached"
	)
	if attached.size() < 2:
		_fail("Expected two independent raster attachments: %s" % [snapshots])
		return false
	for snapshot in attached:
		if int(snapshot.get("coordinate_id", -1)) != 0:
			_fail("Unexpected overlay texture-coordinate ID: %s" % [snapshot])
			return false
		if int(snapshot.get("coordinate_index", -1)) != 0:
			_fail("Overlay was not mapped to Godot UV: %s" % [snapshot])
			return false
		if not bool(snapshot.get("texture_valid", false)):
			_fail("Overlay callback did not retain its prepared texture")
			return false
		if not bool(snapshot.get("material_valid", false)):
			_fail("Overlay callback ran before material application: %s" % [snapshot])
			return false
		if not bool(snapshot.get("shader_texture_valid", false)):
			_fail("Overlay was not propagated into its custom ShaderMaterial: %s" % [snapshot])
			return false
		if (
			str(snapshot.get("key", "")) == "OverlayB" and
			not bool(snapshot.get("overlay_a_still_bound", false))
		):
			_fail("Attaching OverlayB discarded OverlayA shader state")
			return false
		if not bool(snapshot.get("primitive_valid", false)):
			_fail("Overlay callback did not identify its tile primitive")
			return false
		if not bool(snapshot.get("attached", false)):
			_fail("Overlay callback ran after binding was marked detached")
			return false
		var translation := snapshot.get("translation", Vector2.ZERO) as Vector2
		var scale := snapshot.get("scale", Vector2.ZERO) as Vector2
		if translation == Vector2.ZERO or scale == Vector2.ZERO:
			_fail("Overlay callback lost its UV transform: %s" % [snapshot])
			return false
	var loaded_tile := receiver.primitive_snapshot.get("loaded_tile") as Cesium3DTile
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	var diagnostics: Dictionary = primitive.get_render_diagnostics()
	var overlay_diagnostics: Array = diagnostics.get("raster_overlays", [])
	if (
		str(diagnostics.get("active_material_origin", "")) !=
			"raster_overlay_composite" or
		overlay_diagnostics.size() != 2
	):
		_fail("Overlay composite was not identified in diagnostics: %s" % [diagnostics])
		return false
	for value in overlay_diagnostics:
		var overlay := value as Dictionary
		if (
			not bool(overlay.get("active", false)) or
			int(overlay.get("texture_coordinate_index", -1)) != 0 or
			str(overlay.get("source_semantic", "")) != "_CESIUMOVERLAY_0" or
			not bool(
				(overlay.get("texture", {}) as Dictionary).get("available", false)
			)
		):
			_fail("Active overlay diagnostics were incomplete: %s" % [overlay])
			return false
	return true


func _check_overlay_replacement(
	primitive: CesiumLoadedTilePrimitive,
	loaded_tile: Cesium3DTile
) -> bool:
	var old_binding := overlay_binding_b
	var old_texture := overlay_texture_b
	var previous_event_count: int = receiver.events.size()
	overlay_texture_b = _make_texture(Color(0.2, 0.9, 0.4, 1.0))
	overlay_binding_b = tileset.attach_raster_overlay(
		primitive,
		"OverlayB",
		overlay_texture_b,
		0,
		0,
		Vector2(0.375, 0.125),
		Vector2(0.5, 0.5)
	)
	if overlay_binding_b == null:
		_fail("Replacing OverlayB returned no binding")
		return false
	if old_binding.attached:
		_fail("Replacing OverlayB left the old binding marked attached")
		return false
	if old_binding.texture != null or old_binding.material != null:
		_fail("Replacing OverlayB retained old GPU-facing resources")
		return false
	var replacement_events: Array = receiver.events.slice(previous_event_count)
	if replacement_events != [
		"raster_detaching:OverlayB",
		"raster_attached:OverlayB",
	]:
		_fail("Overlay replacement callbacks were not atomic: %s" % [replacement_events])
		return false
	if tileset.detach_raster_overlay(primitive, "OverlayB", old_texture):
		_fail("A stale raster detach removed the replacement binding")
		return false
	var remaining: Array = loaded_tile.get_raster_overlay_bindings(0)
	if remaining.size() != 1 or remaining[0] != overlay_binding_b:
		_fail("OverlayB replacement did not remain the sole active binding")
		return false
	var transformed := overlay_binding_b.transform_texture_coordinate(
		Vector2(0.5, 0.5)
	)
	if not transformed.is_equal_approx(Vector2(0.625, 0.375)):
		_fail("Overlay UV transform does not match Cesium scale-then-translate semantics")
		return false
	return true


func _attach_test_overlays() -> bool:
	var loaded_tile := receiver.primitive_snapshot.get("loaded_tile") as Cesium3DTile
	if loaded_tile == null:
		_fail("Primitive snapshot did not retain its loaded tile")
		return false
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	if primitive == null:
		_fail("Loaded tile did not retain primitive 0 for raster attachment")
		return false

	overlay_texture_a = _make_texture(Color(0.8, 0.2, 0.1, 1.0))
	overlay_texture_b = _make_texture(Color(0.1, 0.4, 0.9, 1.0))
	var attached_a := tileset.attach_raster_overlay(
		primitive,
		"OverlayA",
		overlay_texture_a,
		0,
		0,
		Vector2(0.125, 0.25),
		Vector2(0.5, 0.75)
	)
	overlay_binding_b = tileset.attach_raster_overlay(
		primitive,
		"OverlayB",
		overlay_texture_b,
		0,
		0,
		Vector2(0.25, 0.5),
		Vector2(0.25, 0.5)
	)
	if attached_a == null or overlay_binding_b == null:
		_fail("Generic raster-overlay contract rejected a valid attachment")
		return false
	return true


func _make_texture(color: Color) -> ImageTexture:
	var image := Image.create(4, 4, false, Image.FORMAT_RGBA8)
	image.fill(color)
	return ImageTexture.create_from_image(image)


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
