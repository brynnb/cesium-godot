extends SceneTree

const TIMEOUT_FRAMES := 360


class TrackingReceiver extends Cesium3DTilesetLifecycleEventReceiver:
	var loaded: Dictionary = {}

	func _init() -> void:
		tile_loaded.connect(_on_tile_loaded)

	func _on_tile_loaded(tile: Cesium3DTile) -> void:
		var side := str((tile.tile_extras as Dictionary).get("side", ""))
		if not side.is_empty():
			loaded[side] = int(loaded.get(side, 0)) + 1


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not RenderingServer.has_method("viewport_query_occlusion"):
		print("Occlusion tileset E2E test skipped on stock Godot")
		quit(0)
		return

	var viewport := SubViewport.new()
	viewport.size = Vector2i(320, 180)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.use_occlusion_culling = true
	root.add_child(viewport)
	var world_root := Node3D.new()
	viewport.add_child(world_root)
	var camera := Camera3D.new()
	camera.current = true
	camera.set_perspective(60.0, 0.1, 100.0)
	world_root.add_child(camera)

	var wall := OccluderInstance3D.new()
	var box := BoxOccluder3D.new()
	box.size = Vector3(10.0, 10.0, 1.0)
	wall.occluder = box
	wall.position = Vector3(0.0, 0.0, -5.0)
	world_root.add_child(wall)
	var hidden_bound := AABB(
		Vector3(-0.5, -0.5, -10.5),
		Vector3.ONE
	)
	var occluder_ready := false
	for _frame in range(TIMEOUT_FRAMES):
		await process_frame
		var results: PackedByteArray = RenderingServer.call(
			"viewport_query_occlusion",
			viewport.get_viewport_rid(),
			[hidden_bound]
		)
		if results == PackedByteArray([2]):
			occluder_ready = true
			break
	if not occluder_ready:
		if not OS.has_environment("CESIUM_TEST_REQUIRE_OCCLUSION"):
			print("Occlusion tileset E2E test skipped without a render-capable viewport")
			quit(0)
			return
		_fail("The authored wall never entered Godot's occlusion buffer")
		return

	var receiver := TrackingReceiver.new()
	world_root.add_child(receiver)
	var tileset := Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/occlusion/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 2
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.movement_prediction_enabled = false
	tileset.frustum_culling_enabled = true
	tileset.fog_culling_enabled = false
	tileset.render_tiles_under_camera = false
	tileset.http_cache_enabled = false
	tileset.occlusion_culling_enabled = true
	tileset.delay_refinement_for_occlusion = true
	tileset.lifecycle_event_receiver = receiver
	world_root.add_child(tileset)

	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		if int(receiver.loaded.get("visible", 0)) > 0:
			for _settle in range(30):
				tileset.update_tileset(camera.global_transform)
				await process_frame
			var statistics := tileset.get_streaming_statistics()
			if int(receiver.loaded.get("hidden", 0)) != 0:
				_fail(
					"The tile behind the occluder was loaded: %s native_occluded=%s generation=%s bounds=%s results=[%s,%s,%s]"
					% [
						receiver.loaded,
						statistics.tiles_occluded,
						statistics.occlusion_query_generation,
						statistics.occlusion_query_bound_count,
						statistics.occlusion_visible_result_count,
						statistics.occlusion_occluded_result_count,
						statistics.occlusion_unavailable_result_count,
					]
				)
				return
			if int(statistics.tiles_occluded) <= 0:
				_fail("Native never counted an occluded tile: %s" % statistics)
				return
			viewport.queue_free()
			print("Occlusion tileset E2E test passed: %s" % receiver.loaded)
			quit(0)
			return

	_fail(
		"Visible tile never loaded: loaded=%s stats=%s"
		% [receiver.loaded, tileset.get_streaming_statistics()]
	)


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
