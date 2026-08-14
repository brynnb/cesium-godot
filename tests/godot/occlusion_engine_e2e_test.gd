extends SceneTree

const TIMEOUT_FRAMES := 240


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not RenderingServer.has_method("viewport_query_occlusion"):
		if OS.has_environment("CESIUM_TEST_REQUIRE_OCCLUSION"):
			_fail("Custom Godot occlusion query API is unavailable")
		else:
			print("Occlusion engine E2E test skipped on stock Godot")
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

	var hidden := AABB(Vector3(-1.0, -1.0, -11.0), Vector3(2.0, 2.0, 2.0))
	var visible := AABB(Vector3(-0.5, -0.5, -2.5), Vector3.ONE)
	var last_results := PackedByteArray()
	for _frame in range(TIMEOUT_FRAMES):
		await process_frame
		var results: PackedByteArray = RenderingServer.call(
			"viewport_query_occlusion",
			viewport.get_viewport_rid(),
			[hidden, visible]
		)
		last_results = results
		if results == PackedByteArray([2, 1]):
			viewport.queue_free()
			print("Occlusion engine E2E test passed")
			quit(0)
			return

	viewport.queue_free()
	if not OS.has_environment("CESIUM_TEST_REQUIRE_OCCLUSION"):
		print("Occlusion engine E2E test skipped without a render-capable viewport")
		quit(0)
		return
	_fail(
		"Godot never reported the hidden/visible bound pair; last=%s renderer=%s"
		% [last_results, RenderingServer.get_current_rendering_method()]
	)


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
