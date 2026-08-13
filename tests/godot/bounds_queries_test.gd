extends SceneTree

const LOAD_TIMEOUT_FRAMES := 300

var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "BoundsQueriesTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(7_000_000.0, 0.0, 0.0)
	test_root.add_child(camera)
	georeference = CesiumGeoreference.new()
	georeference.name = "BoundsGeoreference"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	if not await _check_region():
		return
	if not await _check_s2():
		return
	if not await _check_cylinder():
		return

	print("Cesium bounding-volume variant tests passed")
	test_root.free()
	quit(0)


func _load_bounds(fixture_name: String) -> CesiumBoundingVolume:
	var tileset := Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/bounds/%s_tileset.json" % fixture_name
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	georeference.add_child(tileset)
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		var bounds := tileset.get_tileset_bounds() as CesiumBoundingVolume
		if bounds != null:
			georeference.remove_child(tileset)
			tileset.free()
			return bounds
		await process_frame
	tileset.free()
	return null


func _check_region() -> bool:
	var bounds := await _load_bounds("region")
	if (
		bounds == null or bounds.volume_type != CesiumBoundingVolume.Region or
		bounds.volume_type_name != "region"
	):
		_fail("Region tileset did not expose a geographic region")
		return false
	var details := bounds.details
	var radians: PackedFloat64Array = details.get("rectangle_radians", [])
	var degrees: PackedFloat64Array = details.get("rectangle_degrees", [])
	if (
		radians.size() != 4 or degrees.size() != 4 or
		absf(radians[0] + 0.01) > 0.000001 or
		absf(radians[3] - 0.04) > 0.000001 or
		absf(degrees[0] + 0.5729577951) > 0.0001 or
		float(details.get("minimum_height", -1.0)) != 10.0 or
		float(details.get("maximum_height", -1.0)) != 250.0
	):
		_fail("Region radians/degrees/heights were not preserved: %s" % [details])
		return false
	return true


func _check_s2() -> bool:
	var bounds := await _load_bounds("s2")
	if (
		bounds == null or bounds.volume_type != CesiumBoundingVolume.S2Cell or
		bounds.volume_type_name != "s2_cell"
	):
		_fail("S2 tileset did not expose an S2-cell volume")
		return false
	var details := bounds.details
	if (
		str(details.get("s2_cell_token", "")) != "3" or
		int(details.get("s2_cell_level", -1)) != 0 or
		float(details.get("minimum_height", -1.0)) != 5.0 or
		float(details.get("maximum_height", -1.0)) != 500.0 or
		bounds.center_components.size() != 3
	):
		_fail("S2 token/level/heights were not preserved: %s" % [details])
		return false
	return true


func _check_cylinder() -> bool:
	var bounds := await _load_bounds("cylinder")
	if (
		bounds == null or
		bounds.volume_type != CesiumBoundingVolume.CylinderRegion or
		bounds.volume_type_name != "cylinder_region"
	):
		_fail("Cylinder tileset did not expose a cylinder-region volume")
		return false
	var details := bounds.details
	var translation: PackedFloat64Array = details.get(
		"translation_components", []
	)
	var radial: PackedFloat64Array = details.get("radial_bounds", [])
	var angular: PackedFloat64Array = details.get("angular_bounds", [])
	var rotation: PackedFloat64Array = details.get("rotation_components", [])
	if (
		translation != PackedFloat64Array([100.0, 200.0, 300.0]) or
		radial != PackedFloat64Array([10.0, 50.0]) or
		angular != PackedFloat64Array([-1.25, 0.75]) or
		rotation != PackedFloat64Array([0.0, 0.0, 0.0, 1.0]) or
		float(details.get("height", -1.0)) != 120.0
	):
		_fail("Cylinder shape/transform details were not preserved: %s" % [details])
		return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
