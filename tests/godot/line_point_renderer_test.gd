extends SceneTree

const FIXTURE := "res://fixtures/line_points/tileset.json"
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600

var test_root: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	for type_name in ["CesiumPointCloudShading", "CesiumLoadedTilePrimitive"]:
		if not ClassDB.class_exists(type_name):
			_fail("Missing line/point renderer class: %s" % type_name)
			return
	if not _check_point_cloud_math():
		return
	if not await _check_streamed_topologies():
		return
	print("Cesium line and point renderer test passed")
	quit(0)


func _check_point_cloud_math() -> bool:
	var shading := CesiumPointCloudShading.new()
	if shading.attenuation or shading.geometric_error_scale != 1.0:
		_fail("Point-cloud defaults differ from Cesium for Unreal")
		return false
	var estimate := shading.resolve_geometric_error(
		0.0,
		Vector3(10.0, 10.0, 10.0),
		4
	)
	if abs(estimate - pow(250.0, 1.0 / 3.0)) > 0.00001:
		_fail("Point geometric-error fallback did not use cbrt(volume/count)")
		return false
	shading.base_resolution = 0.25
	if abs(shading.resolve_geometric_error(0.0, Vector3.ONE, 1) - 0.25) > 0.00001:
		_fail("Point base resolution did not precede the volume estimate")
		return false
	if abs(shading.resolve_geometric_error(3.0, Vector3.ONE, 1) - 3.0) > 0.00001:
		_fail("Tile geometric error did not precede base resolution")
		return false
	shading.attenuation = true
	shading.maximum_attenuation = 0.0
	var additive: Dictionary = shading.resolve_parameters(
		true, 3.0, Vector3.ONE, 1, 0, 12.0
	)
	var replacement: Dictionary = shading.resolve_parameters(
		false, 3.0, Vector3.ONE, 1, 0, 12.0
	)
	if additive.maximum_point_size != 5.0 or replacement.maximum_point_size != 12.0:
		_fail("Additive/replacement maximum point sizes differ from upstream")
		return false
	shading.geometric_error_scale = -2.0
	shading.maximum_attenuation = -3.0
	shading.base_resolution = -4.0
	if (
		shading.geometric_error_scale != 0.0
		or shading.maximum_attenuation != 0.0
		or shading.base_resolution != 0.0
	):
		_fail("Point-cloud settings did not clamp invalid negative values")
		return false
	var unsupported_material := ShaderMaterial.new()
	unsupported_material.shader = Shader.new()
	unsupported_material.shader.code = "shader_type spatial;"
	if shading.apply_to_material(
		unsupported_material, false, 1.0, Vector3.ONE, 1, 0, 12.0
	):
		_fail("A custom shader without the point-cloud contract was accepted")
		return false
	var supported_material := ShaderMaterial.new()
	supported_material.shader = Shader.new()
	supported_material.shader.code = """
shader_type spatial;
uniform bool cesium_point_attenuation_enabled;
uniform float cesium_point_geometric_error;
uniform float cesium_point_maximum_size;
uniform float cesium_point_diameter;
"""
	if not shading.apply_to_material(
		supported_material, false, 1.0, Vector3.ONE, 1, 0, 12.0
	):
		_fail("A custom shader with the point-cloud contract was rejected")
		return false
	return true


func _check_streamed_topologies() -> bool:
	test_root = Node3D.new()
	test_root.name = "LinePointRendererTest"
	root.add_child(test_root)
	var camera := Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 5.0, 50.0, 100.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 5.0, 5.0, 5.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var tileset := Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(FIXTURE)
	tileset.maximum_screen_space_error = 12.0
	tileset.maximum_simultaneous_tile_loads = 2
	tileset.main_thread_loading_time_limit_ms = 0.1
	# This must remain harmless for a point/line-only model.
	tileset.create_physics_meshes = true
	var shading: CesiumPointCloudShading = tileset.point_cloud_shading
	shading.attenuation = true
	shading.geometric_error_scale = 2.0
	georeference.add_child(tileset)

	var tile := await _pump_until_realized(tileset, camera)
	if tile == null:
		_fail("Timed out waiting for the line/point fixture")
		return false
	if tile.get_loaded_tile_primitive_count() != 4:
		_fail("Not every non-triangle glTF primitive was realized")
		return false
	var expected_modes := ["points", "lines", "line_loop", "line_strip"]
	var expected_godot_modes := [
		Mesh.PRIMITIVE_POINTS,
		Mesh.PRIMITIVE_LINES,
		Mesh.PRIMITIVE_LINES,
		Mesh.PRIMITIVE_LINE_STRIP,
	]
	var expected_index_counts := [4, 4, 8, 4]
	for index in range(4):
		var primitive: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(index)
		if primitive == null or primitive.primitive_mode_name != expected_modes[index]:
			_fail("Source topology was not preserved in the lifecycle context")
			return false
		if (
			tile.mesh.surface_get_primitive_type(primitive.mesh_surface_index)
			!= expected_godot_modes[index]
		):
			_fail("Source topology mapped to the wrong Godot primitive mode")
			return false
		if (
			tile.mesh.surface_get_array_index_len(primitive.mesh_surface_index)
			!= expected_index_counts[index]
		):
			_fail("Topology index conversion produced the wrong element count")
			return false
		if index == 0 and not primitive.point_primitive:
			_fail("Point primitive classification was false")
			return false
		if index > 0 and not primitive.line_primitive:
			_fail("Line primitive classification was false")
			return false

	var point: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(0)
	if (
		point.point_count != 4
		or point.point_diameter != 7
		or point.primitive_dimensions.distance_to(Vector3(10.0, 10.0, 10.0))
			> 0.0001
		or not point.uses_additive_refinement
		or abs(point.tile_geometric_error - 8.0) > 0.0001
	):
		_fail("Point primitive omitted upstream attenuation inputs: %s" % [
			point.render_diagnostics
		])
		return false
	var material := point.active_material as ShaderMaterial
	if material == null or material.shader == null:
		_fail("Point primitive did not receive a generated ShaderMaterial")
		return false
	var shader_code := material.shader.code
	if not shader_code.contains("POINT_SIZE") or not shader_code.contains("POINT_COORD"):
		_fail("Point shader omitted native GPU attenuation or circular masking")
		return false
	var parameters: Dictionary = point.point_cloud_parameters
	if (
		parameters.attenuation != true
		or abs(float(parameters.geometric_error) - 16.0) > 0.0001
		or abs(float(parameters.maximum_point_size) - 5.0) > 0.0001
		or parameters.diameter != 7
	):
		_fail("Resolved point-cloud shader parameters were incorrect: %s" % [parameters])
		return false
	var unsupported: PackedStringArray = material.get_meta(
		"cesium_unsupported_material_extensions",
		PackedStringArray()
	)
	if unsupported.has("BENTLEY_materials_point_style"):
		_fail("Supported Bentley point styling was reported as unsupported")
		return false
	for child in tile.get_children():
		if child is StaticBody3D:
			_fail("A point/line-only tile incorrectly created triangle collision")
			return false

	var material_id := material.get_instance_id()
	shading.maximum_attenuation = 13.0
	shading.geometric_error_scale = 0.5
	if point.active_material.get_instance_id() != material_id:
		_fail("A live shading edit rebuilt the point material")
		return false
	parameters = point.point_cloud_parameters
	if (
		abs(float(parameters.maximum_point_size) - 13.0) > 0.0001
		or abs(float(parameters.geometric_error) - 4.0) > 0.0001
	):
		_fail("Live point-cloud settings did not update existing tile uniforms")
		return false

	test_root.free()
	test_root = null
	await process_frame
	return true


func _pump_until_realized(
	tileset: Cesium3DTileset,
	camera: Camera3D
) -> Cesium3DTile:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		for child in tileset.get_children():
			if child is Cesium3DTile and child.get_loaded_tile_primitive_count() == 4:
				return child
	return null


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
