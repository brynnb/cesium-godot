extends SceneTree

const LOAD_TIMEOUT_FRAMES := 900
const EARTH_RADIUS := 6_378_137.0
const MetadataStyleReceiver := preload("res://metadata_style_receiver.gd")

var test_root: Node3D
var camera: Camera3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "MetadataStylingTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	test_root.add_child(camera)

	if not _check_rule_evaluation():
		return
	if not await _check_mesh_and_texture_feature_styles():
		return
	if not await _check_instance_feature_style():
		return

	print("Cesium live metadata styling test passed")
	test_root.free()
	quit(0)


func _check_rule_evaluation() -> bool:
	var style := CesiumMetadataStyle.new()
	style.default_show = true
	style.default_color = Color(0.25, 0.5, 0.75, 1.0)
	style.default_color_mix = 0.2
	style.rules = [
		{
			"property": "height",
			"operator": "between",
			"value": 10,
			"maximum": 20,
			"show": false,
			"color": Color.RED,
			"color_mix": 1.0,
		},
		{
			"property": "name",
			"operator": "contains",
			"value": "oak",
			"color": Color.GREEN,
		},
	]
	var first := style.evaluate_feature(7, {"height": 15, "name": "oak"})
	if (
		first.get("show", true) or
		first.get("color", Color.BLACK) != Color.RED or
		first.get("rule_index", -1) != 0
	):
		_fail("Metadata styles did not use deterministic first-match rule order")
		return false
	var second := style.evaluate_feature(8, {"height": 30, "name": "oak tree"})
	if (
		second.get("color", Color.BLACK) != Color.GREEN or
		second.get("rule_index", -1) != 1
	):
		_fail("Metadata style string matching did not resolve the second rule")
		return false
	var fallback := style.evaluate_feature(9, {"height": 30, "name": "pine"})
	if (
		not fallback.get("show", false) or
		fallback.get("color", Color.BLACK) != Color(0.25, 0.5, 0.75, 1.0) or
		not is_equal_approx(float(fallback.get("color_mix", -1.0)), 0.2)
	):
		_fail("Metadata style defaults were not preserved for unmatched features")
		return false
	return true


func _check_mesh_and_texture_feature_styles() -> bool:
	camera.position = Vector3(50.0, 100.0, 150.0)
	camera.look_at(Vector3(50.0, 0.0, 50.0), Vector3.UP)
	var georeference := CesiumGeoreference.new()
	georeference.name = "MeshStyleCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var style := CesiumMetadataStyle.new()
	style.enabled = true
	style.feature_source = CesiumMetadataStyle.MeshFeatures
	style.feature_id_set_index = 0
	style.rules = [{
		"property": "transformed",
		"operator": "greater",
		"value": 3.0,
		"show": false,
		"color": Color.RED,
		"color_mix": 1.0,
	}]
	var tileset := _create_tileset(
		"MeshMetadataStyleTileset",
		"res://fixtures/structural_metadata/tileset.json",
		style
	)
	var receiver := MetadataStyleReceiver.new()
	receiver.name = "MetadataStyleReceiver"
	test_root.add_child(receiver)
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)
	var loaded_tile := await _wait_for_tile(tileset)
	if loaded_tile == null:
		_fail("Timed out streaming the mesh metadata-style fixture")
		return false
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	if not _check_style_diagnostics(primitive, "vertex", 3, "_IMPLICIT_FEATURE_ID"):
		return false
	if receiver.loaded_primitives.size() != 1:
		_fail("Metadata styling did not coexist with lifecycle material selection")
		return false
	var material := primitive.active_material as ShaderMaterial
	if material == null:
		_fail("Mesh metadata style did not retain a ShaderMaterial")
		return false
	var arrays := loaded_tile.mesh.surface_get_arrays(
		primitive.mesh_surface_index
	)
	var encoded_ids := arrays[Mesh.ARRAY_CUSTOM0] as PackedFloat32Array
	if encoded_ids.size() != 9:
		_fail("Mesh feature IDs were not uploaded as one RGB custom value per vertex")
		return false
	if (
		encoded_ids.slice(0, 3) != PackedFloat32Array([0.0, 0.0, 1.0]) or
		encoded_ids.slice(3, 6) != PackedFloat32Array([1.0, 0.0, 1.0]) or
		encoded_ids.slice(6, 9) != PackedFloat32Array([2.0, 0.0, 1.0])
	):
		_fail("Implicit mesh feature IDs were encoded incorrectly: %s" % [encoded_ids])
		return false
	var first_controls := material.get_shader_parameter(
		"cesium_feature_style_controls"
	) as Texture2D
	if first_controls == null or first_controls.get_image() == null:
		_fail("Mesh metadata style did not create its control lookup texture")
		return false
	if first_controls.get_image().get_data().slice(0, 6) != PackedByteArray([
		255, 0,
		0, 255,
		0, 255,
	]):
		_fail("Property-table rules were not encoded into the expected controls")
		return false
	if style.lookup_cache_count != 1 or style.lookup_cache_bytes != 24:
		_fail(
			"Metadata lookup cache accounting was incorrect: count=%d bytes=%d"
			% [style.lookup_cache_count, style.lookup_cache_bytes]
		)
		return false

	# Rule-only edits must replace only the small lookup textures. They must not
	# recreate the Cesium tileset, tile node, mesh, shader, or material.
	var original_tile_id := loaded_tile.get_instance_id()
	var original_mesh_id := loaded_tile.mesh.get_instance_id()
	var original_material_id := material.get_instance_id()
	var original_shader_id := material.shader.get_instance_id()
	var original_controls_id := first_controls.get_instance_id()
	style.rules = [{
		"property": "feature_id",
		"operator": "equals",
		"value": 0,
		"show": false,
		"color": Color.BLUE,
		"color_mix": 0.5,
	}]
	await process_frame
	if (
		loaded_tile.get_instance_id() != original_tile_id or
		loaded_tile.mesh.get_instance_id() != original_mesh_id or
		material.get_instance_id() != original_material_id or
		material.shader.get_instance_id() != original_shader_id
	):
		_fail("A rule-only metadata style edit rebuilt streamed render resources")
		return false
	var updated_controls := material.get_shader_parameter(
		"cesium_feature_style_controls"
	) as Texture2D
	var updated_colors := material.get_shader_parameter(
		"cesium_feature_style_colors"
	) as Texture2D
	if (
		updated_controls == null or updated_colors == null or
		updated_controls.get_instance_id() == original_controls_id
	):
		_fail("A live style edit did not replace its lookup textures")
		return false
	if updated_controls.get_image().get_data().slice(0, 6) != PackedByteArray([
		0, 128,
		255, 0,
		255, 0,
	]):
		_fail("Live metadata rule controls were not refreshed in place")
		return false
	if updated_colors.get_image().get_data().slice(0, 4) != PackedByteArray([
		0, 0, 255, 255,
	]):
		_fail("Live metadata rule color was not refreshed in place")
		return false

	# A fully replacement shader needs the documented metadata contract. Reject
	# an incompatible one deterministically, then restore the generated material.
	var incompatible_shader := Shader.new()
	incompatible_shader.code = "shader_type spatial; void fragment() { ALBEDO = vec3(1.0); }"
	var incompatible_material := ShaderMaterial.new()
	incompatible_material.shader = incompatible_shader
	primitive.render_node.material_override = incompatible_material
	tileset.refresh_metadata_style()
	var incompatible_diagnostics := primitive.get_render_diagnostics().get(
		"metadata_style",
		{}
	) as Dictionary
	if (
		incompatible_diagnostics.get("applied", true) or
		incompatible_diagnostics.get("error", "") !=
		"active custom shader does not implement the Cesium metadata-style contract"
	):
		_fail("Incompatible custom metadata shader was not rejected clearly")
		return false
	primitive.render_node.material_override = null
	tileset.refresh_metadata_style()
	if not primitive.get_render_diagnostics().get(
		"metadata_style",
		{}
	).get("applied", false):
		_fail("Generated metadata material did not recover after a custom override")
		return false

	# Encoding-selection edits intentionally create a new tileset generation.
	# Select the texture-backed set by its Unreal-compatible generated name.
	style.feature_id_set_name = "_FEATURE_ID_TEXTURE_0"
	loaded_tile = await _wait_for_tile(tileset, "texture", original_tile_id)
	if loaded_tile == null:
		_fail("Timed out recreating the texture-backed metadata-style generation")
		return false
	primitive = loaded_tile.get_loaded_tile_primitive(0)
	if not _check_style_diagnostics(
		primitive,
		"texture",
		3,
		"_FEATURE_ID_TEXTURE_0"
	):
		return false
	material = primitive.active_material as ShaderMaterial
	if material.get_shader_parameter("cesium_feature_id_texture") == null:
		_fail("Texture-backed feature IDs were not bound for nearest GPU sampling")
		return false

	tileset.free()
	georeference.free()
	receiver.free()
	await process_frame
	return true


func _check_instance_feature_style() -> bool:
	camera.position = Vector3(EARTH_RADIUS + 40.0, 50.0, 200.0)
	camera.look_at(Vector3(EARTH_RADIUS + 40.0, 40.0, 0.0), Vector3.UP)
	var georeference := CesiumGeoreference.new()
	georeference.name = "InstanceStyleCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var style := CesiumMetadataStyle.new()
	style.enabled = true
	style.feature_source = CesiumMetadataStyle.InstanceFeatures
	style.feature_id_set_name = "trees"
	style.rules = [{
		"property": "height_cm",
		"operator": "greater_or_equal",
		"value": 300,
		"show": false,
	}]
	var tileset := _create_tileset(
		"InstanceMetadataStyleTileset",
		"res://fixtures/instancing/tileset.json",
		style
	)
	georeference.add_child(tileset)
	var loaded_tile := await _wait_for_tile(tileset)
	if loaded_tile == null:
		_fail("Timed out streaming the instance metadata-style fixture")
		return false
	var primitive := loaded_tile.get_loaded_tile_primitive(0)
	if not _check_style_diagnostics(primitive, "instance", 4, "trees"):
		return false
	var component := primitive.render_node as CesiumGltfInstancedComponent
	if component == null or component.multimesh == null:
		_fail("Instance metadata style did not retain its MultiMesh component")
		return false
	var multi_mesh := component.multimesh
	if not multi_mesh.use_custom_data or multi_mesh.buffer.size() != 64:
		_fail("Instance feature IDs did not extend the packed MultiMesh buffer")
		return false
	var buffer := multi_mesh.buffer
	var expected_ids := [2.0, 0.0, 3.0, 1.0]
	for index in range(4):
		var offset := index * 16 + 12
		if (
			buffer[offset] != expected_ids[index] or
			buffer[offset + 1] != 0.0 or
			buffer[offset + 2] != 1.0
		):
			_fail("Instance feature ID %d was packed incorrectly" % index)
			return false
	var material := primitive.active_material as ShaderMaterial
	var controls := material.get_shader_parameter(
		"cesium_feature_style_controls"
	) as Texture2D
	if controls == null or controls.get_image().get_data() != PackedByteArray([
		255, 0,
		255, 0,
		0, 255,
		0, 255,
	]):
		_fail("Instance property-table rules produced the wrong lookup controls")
		return false

	# The same mixed tile intentionally contains primitives without instance
	# features. Their diagnostics must explain the per-primitive mismatch while
	# valid instanced primitives continue to render and style normally.
	var ordinary := loaded_tile.get_loaded_tile_primitive(2)
	var ordinary_style := ordinary.get_render_diagnostics().get(
		"metadata_style",
		{}
	) as Dictionary
	if (
		not ordinary_style.get("requested", false) or
		ordinary_style.get("encoded", true) or
		str(ordinary_style.get("encoding_diagnostic", "")).is_empty()
	):
		_fail("Mixed-content instance style failure was not diagnosable")
		return false

	tileset.free()
	georeference.free()
	await process_frame
	return true


func _create_tileset(
	tileset_name: String,
	fixture: String,
	style: CesiumMetadataStyle
) -> Cesium3DTileset:
	var result := Cesium3DTileset.new()
	result.name = tileset_name
	result.data_source = Cesium3DTileset.FromUrl
	result.url = "file://" + ProjectSettings.globalize_path(fixture)
	result.create_physics_meshes = false
	result.maximum_screen_space_error = 1.0
	result.maximum_simultaneous_tile_loads = 2
	result.preload_ancestors = false
	result.preload_siblings = false
	result.metadata_style = style
	return result


func _wait_for_tile(
	tileset: Cesium3DTileset,
	expected_source := "",
	excluded_instance_id := 0
) -> Cesium3DTile:
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		for child in tileset.get_children():
			var tile := child as Cesium3DTile
			if (
				tile == null or not tile.visible or
				tile.get_instance_id() == excluded_instance_id or
				tile.get_loaded_tile_primitive_count() == 0
			):
				continue
			if not expected_source.is_empty():
				var diagnostics := tile.get_loaded_tile_primitive(0).get_render_diagnostics()
				var metadata_style := diagnostics.get("metadata_style", {}) as Dictionary
				if metadata_style.get("source", "") != expected_source:
					continue
			return tile
		await process_frame
	return null


func _check_style_diagnostics(
	primitive: CesiumLoadedTilePrimitive,
	expected_source: String,
	expected_count: int,
	expected_name: String
) -> bool:
	if primitive == null:
		_fail("Metadata style fixture produced no loaded primitive")
		return false
	var style := primitive.get_render_diagnostics().get(
		"metadata_style",
		{}
	) as Dictionary
	if (
		not style.get("requested", false) or
		not style.get("encoded", false) or
		not style.get("applied", false) or
		style.get("source", "") != expected_source or
		style.get("feature_count", -1) != expected_count or
		style.get("feature_id_set_name", "") != expected_name or
		not str(style.get("error", "")).is_empty()
	):
		_fail("Metadata style diagnostics were incomplete: %s" % [style])
		return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null:
		test_root.free()
	quit(1)
