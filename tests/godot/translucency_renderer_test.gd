extends SceneTree

const MIXED_FIXTURE := "res://fixtures/translucency/tileset.json"
const ALL_BLEND_FIXTURE := "res://fixtures/translucency/all_blend_tileset.json"
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600

var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not ClassDB.class_exists("CesiumLoadedTilePrimitive"):
		_fail("Missing CesiumLoadedTilePrimitive")
		return
	test_root = Node3D.new()
	test_root.name = "TranslucencyRendererTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 120.0, 0.0, 120.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS, 0.0, 0.0), Vector3.UP)
	georeference = CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	if not await _check_mixed_alpha():
		return
	if not await _check_all_blend_without_prepass():
		return

	print("Cesium translucent primitive renderer test passed")
	test_root.free()
	quit(0)


func _make_tileset(fixture: String) -> Cesium3DTileset:
	var tileset := Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(fixture)
	tileset.create_physics_meshes = true
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 2
	tileset.main_thread_loading_time_limit_ms = 0.1
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	return tileset


func _check_mixed_alpha() -> bool:
	var tileset := _make_tileset(MIXED_FIXTURE)
	tileset.translucency_sort_priority = 7
	georeference.add_child(tileset)
	var tile := await _pump_until_realized(tileset)
	if tile == null:
		_fail("Timed out waiting for the mixed-alpha fixture")
		return false
	if tile.mesh == null or tile.mesh.get_surface_count() != 1:
		_fail("Opaque batching did not retain exactly one parent surface")
		return false

	var opaque: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(0)
	var blend_a: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(1)
	var blend_b: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(2)
	if opaque == null or blend_a == null or blend_b == null:
		_fail("Mixed-alpha fixture omitted a logical primitive")
		return false
	if opaque.translucent or opaque.translucency_isolated or opaque.render_node != tile:
		_fail("Opaque primitive left the batched parent renderer")
		return false
	if tile.get_loaded_tile_primitive_for_mesh_surface(0) != opaque:
		_fail("Opaque parent surface no longer maps to its logical primitive")
		return false
	if (
		not blend_a.translucent or not blend_b.translucent
		or not blend_a.translucency_isolated
		or not blend_b.translucency_isolated
	):
		_fail("glTF BLEND primitives were not marked as isolated translucency")
		return false
	if (
		blend_a.render_node == tile or blend_b.render_node == tile
		or blend_a.render_node == blend_b.render_node
		or not (blend_a.render_node is MeshInstance3D)
		or not (blend_b.render_node is MeshInstance3D)
	):
		_fail("BLEND primitives did not receive independent render nodes")
		return false
	for blend in [blend_a, blend_b]:
		var render_node := blend.render_node as MeshInstance3D
		if (
			render_node == null or render_node.get_parent() != tile
			or not render_node.sorting_use_aabb_center
			or render_node.mesh == null
			or render_node.mesh.get_surface_count() != 1
			or blend.mesh_surface_index != 0
		):
			_fail("Independent BLEND renderer has incorrect mesh or sort state")
			return false
		var material := blend.active_material as ShaderMaterial
		if material == null or material.shader == null:
			_fail("BLEND primitive did not receive a generated shader material")
			return false
		if (
			material.render_priority != 7
			or material.get_meta("cesium_alpha_mode", "") != "BLEND"
			or not material.get_meta("cesium_source_translucent", false)
			or not material.get_meta("cesium_translucency_depth_prepass", false)
			or not material.shader.code.contains("depth_prepass_alpha")
			or not material.shader.code.contains("ALPHA =")
		):
			_fail("Generated BLEND material omitted alpha/depth/sort policy")
			return false
		var diagnostics: Dictionary = blend.get_render_diagnostics()
		if (
			not diagnostics.get("translucent", false)
			or not diagnostics.get("translucency_isolated", false)
			or not diagnostics.get("sorting_uses_aabb_center", false)
		):
			_fail("Translucency diagnostics do not describe realized state")
			return false

	await physics_frame
	if not _check_collision_and_picking(tile, [opaque, blend_a, blend_b]):
		return false

	var material_ids: Array[int] = []
	for primitive in [opaque, blend_a, blend_b]:
		material_ids.push_back(primitive.active_material.get_instance_id())
	tileset.translucency_sort_priority = -4
	for index in range(3):
		var primitive: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(index)
		if (
			primitive.active_material.get_instance_id() != material_ids[index]
			or primitive.active_material.render_priority != -4
		):
			_fail("Live sort-priority edit rebuilt or missed a material")
			return false

	tileset.free()
	await process_frame
	return true


func _check_all_blend_without_prepass() -> bool:
	var tileset := _make_tileset(ALL_BLEND_FIXTURE)
	tileset.translucency_depth_prepass_enabled = false
	georeference.add_child(tileset)
	var tile := await _pump_until_realized(tileset)
	if tile == null:
		_fail("Timed out waiting for the all-BLEND fixture")
		return false
	if tile.mesh == null or tile.mesh.get_surface_count() != 0:
		_fail("All-BLEND content created duplicate parent render surfaces")
		return false
	var primitives: Array[CesiumLoadedTilePrimitive] = []
	for index in range(3):
		var primitive: CesiumLoadedTilePrimitive = tile.get_loaded_tile_primitive(index)
		if (
			primitive == null or not primitive.translucency_isolated
			or primitive.render_node == tile
		):
			_fail("All-BLEND content did not survive an empty parent mesh")
			return false
		var material := primitive.active_material as ShaderMaterial
		if (
			material == null or material.shader == null
			or material.shader.code.contains("depth_prepass_alpha")
			or material.get_meta("cesium_translucency_depth_prepass", true)
		):
			_fail("Disabled translucency depth prepass remained in the shader")
			return false
		primitives.push_back(primitive)
	await physics_frame
	if not _check_collision_and_picking(tile, primitives):
		return false
	tileset.free()
	await process_frame
	return true


func _check_collision_and_picking(
	tile: Cesium3DTile,
	primitives: Array
) -> bool:
	var collision_body: StaticBody3D
	for child in tile.get_children():
		if child is StaticBody3D:
			collision_body = child
			break
	if collision_body == null or collision_body.get_child_count() != 1:
		_fail("Triangle primitives did not produce one combined collision body")
		return false
	var collision_shape := collision_body.get_child(0) as CollisionShape3D
	var concave := collision_shape.shape as ConcavePolygonShape3D
	if concave == null or concave.get_faces().size() != primitives.size() * 3:
		_fail("Combined collision omitted opaque or translucent triangles")
		return false

	for index in range(primitives.size()):
		var primitive: CesiumLoadedTilePrimitive = primitives[index]
		var renderer := primitive.render_node as MeshInstance3D
		var mesh := renderer.mesh as ArrayMesh
		var vertices: PackedVector3Array = mesh.surface_get_arrays(
			primitive.mesh_surface_index
		)[Mesh.ARRAY_VERTEX]
		var local_hit := (vertices[0] + vertices[1] + vertices[2]) / 3.0
		var local_normal := (
			(vertices[1] - vertices[0]).cross(vertices[2] - vertices[0]).normalized()
		)
		var world_hit := renderer.to_global(local_hit)
		var world_normal := (renderer.global_basis * local_normal).normalized()
		var query := PhysicsRayQueryParameters3D.create(
			world_hit + world_normal * 10.0,
			world_hit - world_normal * 10.0
		)
		var hit := test_root.get_world_3d().direct_space_state.intersect_ray(query)
		if hit.is_empty():
			query = PhysicsRayQueryParameters3D.create(
				world_hit - world_normal * 10.0,
				world_hit + world_normal * 10.0
			)
			hit = test_root.get_world_3d().direct_space_state.intersect_ray(query)
		if hit.is_empty():
			_fail("Could not raycast primitive %d collision" % index)
			return false
		var resolved: Dictionary = CesiumMetadataPicking.resolve_hit(hit)
		if (
			resolved.get("primitive") != primitive
			or resolved.get("surface_index", -1) != index
			or resolved.get("surface_face_index", -1) != 0
		):
			_fail("Collision face did not map to primitive %d: %s" % [index, resolved])
			return false
	return true


func _pump_until_realized(tileset: Cesium3DTileset) -> Cesium3DTile:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		for child in tileset.get_children():
			if (
				child is Cesium3DTile
				and child.visible
				and child.get_loaded_tile_primitive_count() == 3
			):
				return child
	return null


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
