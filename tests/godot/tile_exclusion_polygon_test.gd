extends SceneTree

const EXCLUSION_FIXTURE := "res://fixtures/lifecycle/tileset.json"
const POLYGON_FIXTURE := "res://fixtures/polygon/tileset.json"
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600

var test_root: Node3D
var callback_snapshot: Dictionary = {}
var invalid_callback := false


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	for type_name in [
		"CesiumCartographicPolygon",
		"CesiumPolygonRasterOverlay",
		"CesiumTileExcluder",
		"CesiumTileExclusionContext",
	]:
		if not ClassDB.class_exists(type_name):
			_fail("Missing tile exclusion class: %s" % type_name)
			return
	if not _check_polygon_resource():
		return
	if not await _check_generic_tile_excluder():
		return
	if not await _check_polygon_overlay_and_native_culling():
		return
	print("Cesium tile exclusion and cartographic polygon test passed")
	quit(0)


func _check_polygon_resource() -> bool:
	var polygon := CesiumCartographicPolygon.new()
	if not polygon.set_vertices_degrees_exact(PackedFloat64Array([
		-1.0, -1.0,
		1.0, -1.0,
		1.0, 1.0,
		-1.0, 1.0,
	])):
		_fail("Exact polygon vertices were rejected")
		return false
	if (
		not polygon.is_valid()
		or polygon.vertex_count != 4
		or polygon.get_triangulated_indices().size() != 6
	):
		_fail("Native polygon validation or triangulation was incorrect")
		return false
	var rectangle: PackedFloat64Array = polygon.get_bounding_rectangle_degrees()
	if rectangle.size() != 4 or abs(rectangle[0] + 1.0) > 0.000001:
		_fail("Polygon bounding rectangle lost float64 coordinates: %s" % [rectangle])
		return false
	if not polygon.rectangle_is_completely_inside_degrees(
		PackedFloat64Array([-0.5, -0.5, 0.5, 0.5])
	):
		_fail("Polygon did not recognize an interior rectangle")
		return false
	if not polygon.rectangle_is_completely_outside_degrees(
		PackedFloat64Array([2.0, 2.0, 3.0, 3.0])
	):
		_fail("Polygon did not recognize an exterior rectangle")
		return false

	var ellipsoid := CesiumEllipsoid.new()
	var ecef_components := PackedFloat64Array()
	for vertex in [
		Vector3(-1.0, -1.0, 0.0),
		Vector3(1.0, -1.0, 0.0),
		Vector3(1.0, 1.0, 0.0),
	]:
		ecef_components.append_array(
			ellipsoid.longitude_latitude_height_to_ecef_precise(
				PackedFloat64Array([vertex.x, vertex.y, vertex.z])
			)
		)
	var ecef_polygon := CesiumCartographicPolygon.new()
	if not ecef_polygon.set_vertices_from_ecef_exact(ecef_components, ellipsoid):
		_fail("Exact ECEF polygon conversion failed")
		return false
	var converted: PackedFloat64Array = ecef_polygon.vertices_degrees_exact
	if converted.size() != 6 or abs(converted[0] + 1.0) > 0.000001:
		_fail("ECEF polygon conversion did not preserve longitude/latitude")
		return false
	return true


func _check_generic_tile_excluder() -> bool:
	test_root = Node3D.new()
	test_root.name = "GenericTileExcluderTest"
	root.add_child(test_root)
	var camera := _create_camera(test_root)
	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var tileset := _create_tileset(EXCLUSION_FIXTURE)
	var excluder := CesiumTileExcluder.new()
	excluder.predicate = Callable(self, "_exclude_non_root_tile")
	tileset.add_child(excluder)
	georeference.add_child(tileset)

	for _frame in range(120):
		tileset.update_tileset(camera.global_transform)
		await process_frame
	if (
		not excluder.added_to_tileset
		or excluder.evaluation_count <= 0
		or excluder.excluded_count <= 0
		or callback_snapshot.is_empty()
	):
		_fail("Generic excluder was not called with a typed tile context")
		return false
	if _find_realized_tile(tileset) != null:
		_fail("A tile rejected by the generic excluder was still realized")
		return false
	var transform_components: PackedFloat64Array = callback_snapshot.get(
		"tile_transform_components",
		PackedFloat64Array()
	)
	if (
		not callback_snapshot.has("tile_id")
		or int(callback_snapshot.get("depth", -1)) < 1
		or not (callback_snapshot.get("tile_bounds") is CesiumBoundingVolume)
		or transform_components.size() != 16
	):
		_fail(
			"Tile exclusion context omitted stable tile/bounds/transform facts: %s"
			% [callback_snapshot]
		)
		return false

	invalid_callback = true
	for _frame in range(3):
		tileset.update_tileset(camera.global_transform)
		await process_frame
	if excluder.invalid_result_count <= 0:
		_fail("Invalid predicate results were not diagnosed and included safely")
		return false
	invalid_callback = false
	excluder.enabled = false
	if not await _pump_until_realized(tileset, camera):
		_fail("Disabling the excluder did not make the tile selectable again")
		return false
	test_root.free()
	test_root = null
	await process_frame
	return true


func _check_polygon_overlay_and_native_culling() -> bool:
	test_root = Node3D.new()
	test_root.name = "PolygonOverlayTest"
	root.add_child(test_root)
	var camera := _create_camera(test_root)
	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var tileset := _create_tileset(POLYGON_FIXTURE)
	var polygon := CesiumCartographicPolygon.new()
	polygon.vertices_degrees_exact = PackedFloat64Array([
		-0.1, -0.1,
		0.1, -0.1,
		0.1, 0.1,
		-0.1, 0.1,
	])
	var overlay := CesiumPolygonRasterOverlay.new()
	overlay.polygons = [polygon]
	overlay.exclude_selected_tiles = true
	tileset.add_child(overlay)
	georeference.add_child(tileset)

	for _frame in range(120):
		tileset.update_tileset(camera.global_transform)
		await process_frame
	if not overlay.is_added_to_tileset():
		_fail("Polygon mask provider did not attach to its Native tileset")
		return false
	var config: Dictionary = overlay.get_configuration()
	if (
		config.provider_type != "rasterized_polygons"
		or config.valid_polygon_count != 1
		or not config.tile_excluder_active
	):
		_fail("Polygon provider configuration or Native excluder was incomplete: %s" % [config])
		return false
	if _find_realized_tile(tileset) != null:
		_fail("A tile wholly inside the polygon was not culled before realization")
		return false

	overlay.exclude_selected_tiles = false
	if not await _pump_until_realized(tileset, camera):
		_fail("Mask-only polygon mode did not retain and render selected geometry")
		return false
	if overlay.get_configuration().tile_excluder_active:
		_fail("Mask-only polygon mode retained a stale Native tile excluder")
		return false
	var initial_binding := await _pump_until_clipping_binding(tileset, camera)
	if initial_binding == null:
		_fail("Rasterized polygon mask did not attach to streamed geometry")
		return false
	var initial_texture: Texture2D = initial_binding.texture
	var initial_image := initial_texture.get_image()
	if (
		initial_image == null
		or initial_image.get_width() < 1
		or initial_image.get_pixel(0, 0).r < 0.9
	):
		_fail("Interior polygon mask was not rasterized as selected pixels")
		return false
	var initial_material := initial_binding.material as ShaderMaterial
	if (
		initial_material == null
		or initial_material.get_shader_parameter("clipping_enabled") != true
	):
		_fail("Generated tile material did not enable the clipping-mask contract")
		return false
	var initial_texture_id := initial_texture.get_instance_id()

	polygon.vertices_degrees_exact = PackedFloat64Array([
		10.0, 10.0,
		11.0, 10.0,
		11.0, 11.0,
		10.0, 11.0,
	])
	var exterior_binding := await _pump_until_clipping_binding(
		tileset,
		camera,
		initial_texture_id
	)
	if exterior_binding == null or not overlay.is_added_to_tileset():
		_fail("A live polygon edit did not recreate its overlay generation")
		return false
	var exterior_image := (exterior_binding.texture as Texture2D).get_image()
	if exterior_image == null or exterior_image.get_pixel(0, 0).r > 0.1:
		_fail("Exterior polygon mask was not rerasterized as unselected pixels")
		return false
	test_root.free()
	test_root = null
	await process_frame
	return true


func _exclude_non_root_tile(context: CesiumTileExclusionContext):
	if context.depth < 1:
		return false
	callback_snapshot = context.to_dictionary().duplicate(true)
	if invalid_callback:
		return "invalid-result"
	return true


func _create_camera(parent: Node3D) -> Camera3D:
	var camera := Camera3D.new()
	camera.current = true
	parent.add_child(camera)
	camera.global_position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	return camera


func _create_tileset(fixture: String) -> Cesium3DTileset:
	var tileset := Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(fixture)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	return tileset


func _pump_until_realized(tileset: Cesium3DTileset, camera: Camera3D) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if _find_realized_tile(tileset) != null:
			return true
		await process_frame
	return false


func _pump_until_clipping_binding(
	tileset: Cesium3DTileset,
	camera: Camera3D,
	previous_texture_id: int = 0
) -> CesiumRasterOverlayBinding:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		var tile := _find_realized_tile(tileset)
		if tile != null:
			for candidate in tile.get_all_raster_overlay_bindings():
				var binding := candidate as CesiumRasterOverlayBinding
				if (
					binding != null
					and binding.attached
					and binding.overlay_key == "clipping"
					and binding.texture != null
					and (
						previous_texture_id == 0
						or binding.texture.get_instance_id() != previous_texture_id
					)
				):
					return binding
		await process_frame
	return null


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
