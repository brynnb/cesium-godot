extends SceneTree

const FIXTURE := "res://fixtures/true_origin_axis/tileset.json"
const EXPECTED_POSITION := Vector3(0.0, 0.0, -200.0)
const TIMEOUT_FRAMES := 600

var test_root: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if (
		not ClassDB.class_exists("CesiumGeoreference")
		or not ClassDB.class_exists("Cesium3DTileset")
	):
		_fail("Cesium GDExtension did not load")
		return
	test_root = Node3D.new()
	test_root.name = "TrueOriginAxisTest"
	root.add_child(test_root)

	var camera := Camera3D.new()
	camera.current = true
	test_root.add_child(camera)
	camera.global_position = Vector3(0.0, 50.0, -200.0)
	camera.look_at(EXPECTED_POSITION, Vector3.FORWARD)

	var georeference = ClassDB.instantiate("CesiumGeoreference") as Node3D
	georeference.set("origin_type", 1)
	test_root.add_child(georeference)
	var tileset = ClassDB.instantiate("Cesium3DTileset") as Node3D
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
		_fail(
			"True-origin camera did not select the Cesium Z-up tile from its "
			+ "Godot Y-up position"
		)
		return
	if not realized_tile.global_position.is_equal_approx(EXPECTED_POSITION):
		_fail(
			"True-origin tile translation was not converted to Godot: %s"
			% realized_tile.global_position
		)
		return
	var realized_basis := realized_tile.global_transform.basis
	var scale := realized_basis.get_scale()
	if not scale.is_equal_approx(Vector3(2.0, 3.0, 4.0)):
		_fail("True-origin tile linear transform was not retained: %s" % scale)
		return
	if (
		not realized_basis.x.is_equal_approx(Vector3(0.0, 0.0, -2.0))
		or not realized_basis.y.is_equal_approx(Vector3(-3.0, 0.0, 0.0))
		or not realized_basis.z.is_equal_approx(Vector3(0.0, 4.0, 0.0))
	):
		_fail("True-origin tile rotation was not retained: %s" % realized_basis)
		return
	var primitive: RefCounted = realized_tile.call("get_loaded_tile_primitive", 0)
	if primitive == null:
		_fail("True-origin tile did not expose its realized primitive")
		return
	var absolute_origin := primitive.get("absolute_origin") as Vector3
	var absolute_basis := primitive.get("local_to_absolute_basis") as Basis
	if not absolute_origin.is_equal_approx(realized_tile.global_position):
		_fail("Stable material origin does not match the realized tile frame")
		return
	if not absolute_basis.is_equal_approx(realized_basis):
		_fail("Stable material basis does not match the realized tile frame")
		return

	print(
		"True-origin coordinate test passed: position=",
		realized_tile.global_position,
		" scale=",
		scale
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
