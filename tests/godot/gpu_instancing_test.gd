extends SceneTree

const LOAD_TIMEOUT_FRAMES := 600
const EARTH_RADIUS := 6_378_137.0
const EPSILON := 0.02
const RecordingReceiver := preload("res://recording_lifecycle_receiver.gd")

var test_root: Node3D
var camera: Camera3D
var tileset: Cesium3DTileset
var receiver: Cesium3DTilesetLifecycleEventReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "GpuInstancingTest"
	root.add_child(test_root)

	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 40.0, 50.0, 200.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 40.0, 40.0, 0.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = RecordingReceiver.new()
	test_root.add_child(receiver)

	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/instancing/tileset.json"
	)
	tileset.create_physics_meshes = true
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 2
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	var loaded_tile := await _wait_for_tile()
	if loaded_tile == null:
		_fail("Timed out waiting for the GPU-instanced fixture")
		return
	# Bulk MultiMesh buffers are consumed by RenderingServer. Let its command
	# queue cross one frame before querying the per-instance convenience API.
	await process_frame
	if loaded_tile.mesh == null or loaded_tile.mesh.get_surface_count() != 1:
		_fail("Mixed tile did not keep exactly its one ordinary primitive surface")
		return
	if loaded_tile.get_loaded_tile_primitive_count() != 4:
		_fail("Expected three instanced source primitives and one ordinary primitive")
		return

	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	if primitive == null or not primitive.gpu_instanced:
		_fail("Loaded primitive did not retain its GPU-instanced identity")
		return
	if primitive.instance_count != 4 or primitive.mesh_surface_index != 0:
		_fail("Instanced primitive count or local surface index was wrong")
		return
	var component := primitive.render_node as CesiumGltfInstancedComponent
	if component == null or component.get_parent() != loaded_tile:
		_fail("Instanced primitive was not realized as a child MultiMesh component")
		return
	if component.material_override == null:
		_fail("Lifecycle material override was not applied to the MultiMesh component")
		return
	var instance_features := component.instance_features as CesiumPrimitiveFeatures
	if instance_features == null or primitive.instance_features != instance_features:
		_fail("EXT_instance_features was not retained on the component and primitive")
		return
	if instance_features.instance_count != 4 or instance_features.vertex_count != 0:
		_fail("Instance feature identity was confused with mesh vertex identity")
		return
	if instance_features.get_feature_id_set_count() != 5:
		_fail("Expected all valid and invalid instance feature ID definitions")
		return
	var instance_set := instance_features.get_feature_id_set(0) as CesiumFeatureIdSet
	if (
		instance_set == null or
		not instance_set.is_valid() or
		instance_set.feature_id_set_type_name != "instance" or
		instance_set.instance_count != 4 or
		instance_set.attribute_index != 0 or
		instance_set.property_table_index != 0 or
		instance_set.label != "trees"
	):
		_fail("Instance feature ID set lost its authored definition")
		return
	if [0, 1, 2, 3].map(instance_set.get_feature_id_for_instance) != [2, 0, 3, 1]:
		_fail("Instance feature ID accessor values were decoded incorrectly")
		return
	var implicit_set := instance_features.get_feature_id_set(1) as CesiumFeatureIdSet
	if (
		implicit_set == null or
		not implicit_set.is_valid() or
		implicit_set.feature_id_set_type_name != "instance_implicit" or
		implicit_set.get_feature_id_for_instance(3) != 3
	):
		_fail("Implicit instance IDs were not assigned from instance indices")
		return
	var invalid_implicit := instance_features.get_feature_id_set(2) as CesiumFeatureIdSet
	var invalid_attribute := instance_features.get_feature_id_set(3) as CesiumFeatureIdSet
	if (
		invalid_implicit == null or invalid_implicit.is_valid() or
		invalid_implicit.status_name != "error_invalid_definition" or
		invalid_attribute == null or invalid_attribute.is_valid() or
		invalid_attribute.status_name != "error_invalid_accessor"
	):
		_fail("Invalid instance feature definitions did not remain diagnosable")
		return
	var null_implicit := instance_features.get_feature_id_set(4) as CesiumFeatureIdSet
	var null_pick := CesiumMetadataPicking.resolve_instance(primitive, 3, 4)
	if (
		null_implicit == null or
		not null_implicit.is_valid() or
		null_implicit.get_feature_id_for_instance(3) != 3 or
		not null_implicit.is_null_feature_id(3) or
		null_pick.get("valid", true) or
		not null_pick.get("metadata", {}).is_empty()
	):
		_fail("Implicit null instance ID was not excluded from metadata")
		return
	var model_metadata := primitive.model_metadata as CesiumModelMetadata
	if model_metadata == null or model_metadata.get_property_table_count() != 1:
		_fail("Instance property table was not retained with the primitive")
		return
	if (
		instance_features.get_metadata_values_for_instance(
			model_metadata,
			0
		).get("height_cm", -1) != 300
	):
		_fail("Instance feature ID 2 did not join to property-table row 2")
		return
	var resolved_instance := CesiumMetadataPicking.resolve_instance(primitive, 2)
	if (
		not resolved_instance.get("valid", false) or
		resolved_instance.get("instance_index", -1) != 2 or
		resolved_instance.get("feature_id", -1) != 3 or
		resolved_instance.get("feature_source", "") != "instance" or
		resolved_instance.get("metadata", {}).get("height_cm", -1) != 400
	):
		_fail("Direct instance picking did not resolve feature row 3: %s" % [resolved_instance])
		return
	var synthetic_hit := {
		"primitive": primitive,
		"instance_index": 1,
		# Deliberately no face or position: instance metadata takes precedence
		# over the shared primitive's mesh feature path.
	}
	var resolved_synthetic_hit := CesiumMetadataPicking.resolve_hit(synthetic_hit)
	if (
		resolved_synthetic_hit.get("feature_id", -1) != 0 or
		resolved_synthetic_hit.get("metadata", {}).get("height_cm", -1) != 100
	):
		_fail("Instanced hit did not prefer EXT_instance_features")
		return
	if CesiumMetadataPicking.resolve_instance(primitive, 4).get("valid", true):
		_fail("Out-of-bounds instance picking was not rejected")
		return
	var second_instanced_primitive := loaded_tile.get_loaded_tile_primitive(1)
	if (
		second_instanced_primitive == null or
		not second_instanced_primitive.gpu_instanced or
		second_instanced_primitive.instance_features != instance_features
	):
		_fail("One node's instance metadata was duplicated per mesh primitive")
		return
	var ordinary_primitive := loaded_tile.get_loaded_tile_primitive(2)
	if (
		ordinary_primitive == null or
		ordinary_primitive.gpu_instanced or
		ordinary_primitive.render_node != loaded_tile or
		ordinary_primitive.mesh_surface_index != 0
	):
		_fail("Mixed ordinary primitive lost its parent-mesh render mapping")
		return
	if loaded_tile.get_loaded_tile_primitive_for_mesh_surface(0) != ordinary_primitive:
		_fail("Parent mesh surface did not map back to the correct logical primitive")
		return
	var fallback_primitive := loaded_tile.get_loaded_tile_primitive(3)
	if fallback_primitive == null:
		_fail("Mesh-feature fallback fixture produced no logical primitive")
		return
	var fallback_component := fallback_primitive.render_node as CesiumGltfInstancedComponent
	if (
		not fallback_primitive.gpu_instanced or
		fallback_primitive.instance_features != null or
		fallback_component == null
	):
		_fail("Mesh-feature fallback fixture lost its instanced identity")
		return
	var fallback_mesh := fallback_component.multimesh.mesh
	var fallback_vertices: PackedVector3Array = fallback_mesh.surface_get_arrays(0)[
		Mesh.ARRAY_VERTEX
	]
	var fallback_local_hit := (
		fallback_vertices[0] + fallback_vertices[1] + fallback_vertices[2]
	) / 3.0
	var fallback_component_hit := (
		fallback_component.multimesh.get_instance_transform(0) * fallback_local_hit
	)
	var fallback_hit := {
		"primitive": fallback_primitive,
		"instance_index": 0,
		"face_index": 0,
		"position": fallback_component.to_global(fallback_component_hit),
	}
	var resolved_fallback := CesiumMetadataPicking.resolve_hit(fallback_hit)
	if (
		resolved_fallback.get("feature_source", "") != "mesh" or
		resolved_fallback.get("feature_id", -1) != 0 or
		resolved_fallback.get("metadata", {}).get("height_cm", -1) != 100
	):
		_fail("Instanced hit did not fall back to shared mesh features: %s" % [resolved_fallback])
		return
	await physics_frame
	var ordinary_vertices: PackedVector3Array = loaded_tile.mesh.surface_get_arrays(0)[
		Mesh.ARRAY_VERTEX
	]
	var local_hit := (
		ordinary_vertices[0] + ordinary_vertices[1] + ordinary_vertices[2]
	) / 3.0
	var local_normal := (
		(ordinary_vertices[1] - ordinary_vertices[0]).cross(
			ordinary_vertices[2] - ordinary_vertices[0]
		).normalized()
	)
	var world_hit := loaded_tile.to_global(local_hit)
	var world_normal := (loaded_tile.global_basis * local_normal).normalized()
	var ray_query := PhysicsRayQueryParameters3D.create(
		world_hit + world_normal * 10.0,
		world_hit - world_normal * 10.0
	)
	var ray_hit := test_root.get_world_3d().direct_space_state.intersect_ray(ray_query)
	if ray_hit.is_empty():
		ray_query = PhysicsRayQueryParameters3D.create(
			world_hit - world_normal * 10.0,
			world_hit + world_normal * 10.0
		)
		ray_hit = test_root.get_world_3d().direct_space_state.intersect_ray(ray_query)
	if ray_hit.is_empty():
		_fail("Mixed tile's ordinary primitive did not retain collision")
		return
	var resolved_hit := CesiumMetadataPicking.resolve_hit(ray_hit)
	if (
		resolved_hit.get("primitive") != ordinary_primitive or
		resolved_hit.get("surface_index", -1) != 2 or
		resolved_hit.get("mesh_surface_index", -1) != 0
	):
		_fail("Collision face crossed the instanced/ordinary primitive mapping: %s" % [resolved_hit])
		return
	if receiver.events.slice(0, 13) != [
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
	]:
		_fail("Mixed instanced lifecycle callbacks were missing or out of order: %s" % [receiver.events])
		return
	if not str(receiver.primitive_snapshots[0].default_shader_code).contains("cull_disabled"):
		_fail("Mirrored GPU instance did not select the safe unculled material variant")
		return
	if not component.is_visible_in_tree():
		_fail("Instanced render component was not visible with its selected tile")
		return

	var multi_mesh := component.multimesh
	if multi_mesh == null or multi_mesh.instance_count != 4:
		_fail("MultiMesh did not receive all four instance transforms")
		return
	if multi_mesh.mesh == null or multi_mesh.mesh.get_surface_count() != 1:
		_fail("MultiMesh did not retain one renderable primitive surface")
		return
	if multi_mesh.buffer.size() != 48:
		_fail("Instance transforms were not uploaded through one packed buffer")
		return
	var vertices: PackedVector3Array = (
		multi_mesh.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	)
	if vertices.size() != 3:
		_fail("Instancing expanded the source triangle instead of sharing it")
		return

	var first := _transform_from_buffer(multi_mesh.buffer, 0)
	var second := _transform_from_buffer(multi_mesh.buffer, 1)
	var third := _transform_from_buffer(multi_mesh.buffer, 2)
	if not _near(first.origin, Vector3.ZERO):
		_fail(
			"First local instance transform was not preserved: %s buffer=%s"
			% [first, multi_mesh.buffer.slice(0, 12)]
		)
		return
	if not _near(second.origin, Vector3(30, 0, 0)):
		_fail("Second local instance translation was not preserved")
		return
	if not _near(second.basis * Vector3.RIGHT, Vector3(0, 2, 0)):
		_fail("Normalized integer rotation and nonuniform scale were packed incorrectly")
		return
	if not _near(third.origin, Vector3(0, 40, 0)):
		_fail("Third local instance translation was not preserved")
		return
	if not _near(component.transform.origin, Vector3(15, 0, 27)):
		_fail("Nested node/up-axis transform was not retained on the component")
		return
	var combined_first := component.transform * first
	var combined_second := component.transform * second
	var combined_third := component.transform * third
	if not _near(combined_first.origin, Vector3(15, 0, 27)):
		_fail("First instance did not compose with its component transform")
		return
	if not _near(combined_second.origin, Vector3(45, 0, 27)):
		_fail("Second instance did not compose after the node transform")
		return
	if not _near(combined_second.basis * Vector3.RIGHT, Vector3(0, 0, 2)):
		_fail("Combined rotation, scale, and up-axis transform were incorrect")
		return
	if not _near(combined_third.origin, Vector3(15, 0, 67)):
		_fail("Third instance did not compose after the node transform")
		return
	if component.custom_aabb.size.x < 30.0 or component.custom_aabb.size.y < 40.0:
		_fail("Rendered MultiMesh bounds did not encompass all instances: %s" % [component.custom_aabb])
		return

	var retained_primitive := primitive
	tileset.free()
	tileset = null
	await process_frame
	if retained_primitive.loaded_tile != null or retained_primitive.render_node != null:
		_fail("Retained instanced context kept a dangling scene-node reference")
		return
	if not retained_primitive.gpu_instanced or retained_primitive.instance_count != 4:
		_fail("Retained primitive lost its stable instanced identity")
		return
	var retained_pick := CesiumMetadataPicking.resolve_instance(
		retained_primitive,
		3
	)
	if (
		retained_pick.get("feature_id", -1) != 1 or
		retained_pick.get("metadata", {}).get("height_cm", -1) != 200
	):
		_fail("Instance metadata did not survive source-tile destruction")
		return

	print("Cesium EXT_mesh_gpu_instancing test passed")
	test_root.free()
	quit(0)


func _wait_for_tile() -> Cesium3DTile:
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		for child in tileset.get_children():
			var tile := child as Cesium3DTile
			if tile != null and tile.visible:
				return tile
		await process_frame
	return null


func _near(actual: Vector3, expected: Vector3) -> bool:
	return actual.distance_to(expected) <= EPSILON


func _transform_from_buffer(buffer: PackedFloat32Array, instance_index: int) -> Transform3D:
	var offset := instance_index * 12
	return Transform3D(
		Basis(
			Vector3(buffer[offset], buffer[offset + 4], buffer[offset + 8]),
			Vector3(buffer[offset + 1], buffer[offset + 5], buffer[offset + 9]),
			Vector3(buffer[offset + 2], buffer[offset + 6], buffer[offset + 10])
		),
		Vector3(buffer[offset + 3], buffer[offset + 7], buffer[offset + 11])
	)


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null:
		test_root.free()
	quit(1)
