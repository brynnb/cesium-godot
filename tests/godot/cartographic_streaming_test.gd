extends SceneTree

const FIXTURE := "res://fixtures/cartographic_axis/tileset.json"
const EXPECTED_POSITION := Vector3(100.0, 20.0, -50.0)
const TIMEOUT_FRAMES := 600

var test_root: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "CartographicStreamingTest"
	root.add_child(test_root)

	var camera := Camera3D.new()
	camera.current = true
	test_root.add_child(camera)
	camera.global_position = Vector3(100.0, 120.0, 250.0)
	camera.look_at(EXPECTED_POSITION, Vector3.UP)

	var georeference := ClassDB.instantiate("CesiumGeoreference") as Node3D
	test_root.add_child(georeference)
	georeference.set("origin_type", 0)
	georeference.call(
		"set_origin_longitude_latitude_height_precise",
		PackedFloat64Array([0.0, 0.0, 0.0])
	)
	var tileset := ClassDB.instantiate("Cesium3DTileset") as Node3D
	tileset.set("data_source", 1)
	tileset.set("url", "file://" + ProjectSettings.globalize_path(FIXTURE))
	tileset.set("maximum_screen_space_error", 1.0)
	georeference.add_child(tileset)

	var realized_tile: Node3D
	for _frame in range(TIMEOUT_FRAMES):
		tileset.call("update_tileset", camera.global_transform)
		realized_tile = _find_realized_tile(tileset)
		if realized_tile != null:
			break
		await process_frame
	if realized_tile == null:
		_fail("Cartographic camera did not select the ECEF tile")
		return
	if not realized_tile.global_position.is_equal_approx(EXPECTED_POSITION):
		_fail("Complete ECEF tile translation was not localized: %s" % realized_tile.global_position)
		return
	var basis := realized_tile.global_transform.basis
	if (
		not basis.x.is_equal_approx(Vector3(2.0, 0.0, 0.0))
		or not basis.y.is_equal_approx(Vector3(0.0, -3.0, 0.0))
		or not basis.z.is_equal_approx(Vector3(0.0, 0.0, -4.0))
	):
		_fail("Complete ECEF tile linear transform was not localized: %s" % basis)
		return

	var original_id := realized_tile.get_instance_id()
	var primitive: RefCounted = realized_tile.call("get_loaded_tile_primitive", 0)
	if primitive == null:
		_fail("Cartographic tile did not expose its realized primitive")
		return
	if not georeference.call(
		"set_origin_ecef_precise",
		PackedFloat64Array([6378157.0, 100.0, 50.0])
	):
		_fail("Could not shift the cartographic origin to the realized tile")
		return
	if realized_tile.get_instance_id() != original_id or not is_instance_valid(realized_tile):
		_fail("Origin shift recreated the streamed tile instead of moving it")
		return
	if realized_tile.global_position.length() > 0.001:
		_fail("Origin shift did not precisely recenter the streamed tile: %s" % realized_tile.global_position)
		return
	if primitive.call("get_loaded_tile") != realized_tile:
		_fail("Origin shift invalidated the live primitive context")
		return

	print(
		"Cartographic streaming test passed: initial_position=",
		EXPECTED_POSITION,
		" shifted_position=",
		realized_tile.global_position
	)
	test_root.free()
	quit(0)


func _find_realized_tile(source: Node) -> Node3D:
	var pending: Array[Node] = [source]
	while not pending.is_empty():
		var node: Node = pending.pop_back()
		for child in node.get_children():
			pending.append(child)
		if node.is_class("Cesium3DTile") and (node as Node3D).visible:
			return node as Node3D
	return null


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
