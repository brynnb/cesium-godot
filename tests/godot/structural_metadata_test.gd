extends SceneTree

const RecordingReceiver := preload("res://recording_lifecycle_receiver.gd")
const LOAD_TIMEOUT_FRAMES := 900

const VARIABLE_UINT8 := "example_variable_length_ARRAY_normalized_UINT8"
const FIXED_BOOLEAN := "example_fixed_length_ARRAY_BOOLEAN"
const VARIABLE_STRING := "example_variable_length_ARRAY_STRING"
const FIXED_ENUM := "example_fixed_length_ARRAY_ENUM"

var test_root: Node3D
var camera: Camera3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "StructuralMetadataTest"
	root.add_child(test_root)

	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(0.5, 2.0, 2.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(0.5, 0.0, 0.5), Vector3.UP)

	var repository_root := ProjectSettings.globalize_path("res://../..").simplify_path()
	var fixture_path := repository_root.path_join(
		"build/dependencies/sources/cesium-native/"
		+ "Cesium3DTilesSelection/test/data/ComplexTypes/"
		+ "tileset.json"
	)
	if not FileAccess.file_exists(fixture_path):
		_fail("Missing Cesium Native structural metadata fixture: %s" % fixture_path)
		return

	var receiver := RecordingReceiver.new()
	receiver.name = "StructuralMetadataReceiver"
	test_root.add_child(receiver)

	var georeference := CesiumGeoreference.new()
	georeference.name = "FixtureCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	var tileset := Cesium3DTileset.new()
	tileset.name = "StructuralMetadataTileset"
	tileset.data_source = 1
	tileset.url = "file://" + fixture_path
	tileset.create_physics_meshes = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.has("visibility:true"):
			break
		await process_frame
	if not receiver.events.has("visibility:true"):
		_fail("Timed out streaming the structural metadata fixture")
		return

	if receiver.primitive_snapshots.is_empty():
		_fail("Structural metadata fixture produced no realized primitive")
		return
	var loaded_tile := (
		receiver.primitive_snapshots[0].get("loaded_tile") as Cesium3DTile
	)
	if loaded_tile == null:
		_fail("Structural metadata fixture did not expose its loaded tile")
		return
	var metadata := loaded_tile.model_metadata as CesiumModelMetadata
	if metadata == null or metadata.get_property_table_count() != 1:
		_fail("Expected one model property table")
		return
	if metadata.get_enum_count() != 1:
		_fail("Expected one structural metadata enum definition")
		return
	var enum_definition := metadata.find_enum("exampleEnumType") as CesiumMetadataEnum
	if enum_definition == null or enum_definition.value_type != "UINT16":
		_fail("Enum definition ID or integer value type was not preserved")
		return
	if enum_definition.get_value_count() != 3:
		_fail("Enum definition lost one or more values")
		return
	if enum_definition.get_name_for_value(2) != "ExampleEnumValueC":
		_fail("Enum integer value did not resolve to its authored name")
		return
	if enum_definition.has_value(99) or not enum_definition.get_value_details(99).is_empty():
		_fail("Unknown enum values were not rejected")
		return
	if enum_definition.get_values()[0].get("name", "") != "ExampleEnumValueA":
		_fail("Enum values did not preserve authored order")
		return
	var table := metadata.get_property_table(0) as CesiumPropertyTable
	if table == null or not table.is_valid():
		_fail("Model property table was missing or invalid")
		return
	if table.table_name != "Example property table":
		_fail("Property-table name was not preserved: %s" % table.table_name)
		return
	if table.class_name != "exampleMetadataClass" or table.count != 4:
		_fail("Property-table class or feature count was not preserved")
		return
	var names := table.get_property_names()
	if names.size() != 4:
		_fail("Expected four complex metadata properties, received %s" % [names])
		return

	var uint_property := table.find_property(VARIABLE_UINT8) as CesiumMetadataProperty
	if not _check_property_shape(uint_property, "SCALAR", "UINT8", true, true, 0):
		return
	if uint_property.size != 4:
		_fail("Normalized UINT8 array property lost its feature rows")
		return
	if uint_property.get_raw_value(2) != [0, 85, 170, 255]:
		_fail("Normalized UINT8 raw values were decoded incorrectly")
		return
	var normalized_values := uint_property.get_value(2) as Array
	if not _array_is_approximately(normalized_values, [0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0]):
		_fail("Normalized UINT8 transformed values were decoded incorrectly: %s" % [normalized_values])
		return

	var bool_property := table.find_property(FIXED_BOOLEAN) as CesiumMetadataProperty
	if not _check_property_shape(bool_property, "BOOLEAN", "", true, false, 10):
		return
	if bool_property.get_value(0) != [true, false, true, false, true, false, true, false, true, false]:
		_fail("Fixed BOOLEAN array was decoded incorrectly")
		return

	var string_property := table.find_property(VARIABLE_STRING) as CesiumMetadataProperty
	if not _check_property_shape(string_property, "STRING", "", true, false, 0):
		return
	if string_property.get_value(3) != ["One", "Two", "Theee", "Four"]:
		_fail("Variable STRING array was decoded incorrectly")
		return

	var enum_property := table.find_property(FIXED_ENUM) as CesiumMetadataProperty
	if not _check_property_shape(enum_property, "ENUM", "", true, false, 2):
		return
	if enum_property.enum_type != "exampleEnumType":
		_fail("ENUM property lost its schema enum type")
		return
	if enum_property.get_value(2) != [2, 0]:
		_fail("Fixed ENUM array was decoded incorrectly")
		return

	var feature_values := table.get_metadata_values_for_feature(1)
	if feature_values.size() != 4:
		_fail("Feature-level property lookup did not return all four values")
		return
	if feature_values[VARIABLE_STRING] != ["One", "Two"]:
		_fail("Feature-level property lookup returned the wrong string array")
		return

	# Every returned container is an isolated copy. Script mutation cannot alter
	# the worker-owned snapshot or any later query.
	var mutable_raw := uint_property.get_raw_value(2) as Array
	mutable_raw[0] = 99
	if uint_property.get_raw_value(2) != [0, 85, 170, 255]:
		_fail("Script mutation changed the retained metadata snapshot")
		return

	var loaded_primitive := loaded_tile.get_loaded_tile_primitive(0)
	var primitive_features := (
		loaded_primitive.primitive_features as CesiumPrimitiveFeatures
	)
	if primitive_features == null:
		_fail("Loaded primitive did not expose EXT_mesh_features")
		return
	if primitive_features.vertex_count != 16 or primitive_features.face_count != 8:
		_fail("Primitive feature geometry identity was not preserved")
		return
	if primitive_features.get_feature_id_set_count() != 1:
		_fail("Expected one feature ID set on the complex metadata primitive")
		return
	var feature_set := primitive_features.get_feature_id_set(0) as CesiumFeatureIdSet
	if feature_set == null or not feature_set.is_valid():
		_fail("Feature ID attribute was missing or invalid")
		return
	if feature_set.feature_id_set_type_name != "attribute":
		_fail("Feature ID attribute was classified incorrectly")
		return
	if feature_set.feature_count != 4 or feature_set.property_table_index != 0:
		_fail("Feature ID set lost its count or property-table association")
		return
	if feature_set.attribute_index != 0 or feature_set.vertex_count != 16:
		_fail("Feature ID attribute index or vertex values were not preserved")
		return
	if feature_set.get_feature_id_for_vertex(4) != 1:
		_fail("Feature ID attribute returned the wrong value for vertex 4")
		return
	if primitive_features.get_feature_id_from_face(2) != 1:
		_fail("Face 2 did not resolve to feature ID 1")
		return
	var face_values := primitive_features.get_metadata_values_for_face(
		metadata,
		2
	)
	if face_values.get(VARIABLE_STRING, []) != ["One", "Two"]:
		_fail("Face feature ID did not join to its property-table row")
		return

	# Metadata Resources own CPU snapshots independently from tile Nodes. They
	# remain safe to inspect after Cesium evicts or destroys the source tile.
	tileset.free()
	await process_frame
	georeference.free()
	receiver.free()
	await process_frame
	if metadata.get_property_table_count() != 1:
		_fail("Model metadata did not survive source-tile destruction")
		return
	if table.get_metadata_values_for_feature(3)[VARIABLE_STRING] != ["One", "Two", "Theee", "Four"]:
		_fail("Property table did not survive source-tile destruction")
		return
	if enum_property.get_value(0) != [0, 1]:
		_fail("Metadata property did not survive source-tile destruction")
		return
	if enum_definition.get_name_for_value(1) != "ExampleEnumValueB":
		_fail("Enum definition did not survive source-tile destruction")
		return
	if primitive_features.get_feature_id_from_face(6) != 3:
		_fail("Primitive feature IDs did not survive source-tile destruction")
		return
	if (
		primitive_features.get_metadata_values_for_face(metadata, 6).get(
			VARIABLE_STRING,
			[]
		) != ["One", "Two", "Theee", "Four"]
	):
		_fail("Feature-to-metadata lookup did not survive tile destruction")
		return
	if not await _check_defaults_and_transforms_fixture():
		return

	print("Cesium structural metadata property-table tests passed")
	test_root.free()
	quit(0)


func _check_defaults_and_transforms_fixture() -> bool:
	camera.position = Vector3(50.0, 100.0, 150.0)
	camera.look_at(Vector3(50.0, 0.0, 50.0), Vector3.UP)

	var receiver := RecordingReceiver.new()
	receiver.name = "DefaultsMetadataReceiver"
	test_root.add_child(receiver)
	var georeference := CesiumGeoreference.new()
	georeference.name = "DefaultsCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	var tileset := Cesium3DTileset.new()
	tileset.name = "DefaultsMetadataTileset"
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/structural_metadata/tileset.json"
	)
	tileset.create_physics_meshes = true
	tileset.maximum_screen_space_error = 1.0
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.has("visibility:true"):
			break
		await process_frame
	if not receiver.events.has("visibility:true"):
		_fail("Timed out streaming metadata defaults and transforms")
		return false
	var loaded_tile := (
		receiver.primitive_snapshots[0].get("loaded_tile") as Cesium3DTile
	)
	var table := loaded_tile.model_metadata.get_property_table(0) as CesiumPropertyTable
	var model_metadata := loaded_tile.model_metadata as CesiumModelMetadata
	if model_metadata.get_property_texture_count() != 2:
		_fail("Expected valid and invalid model-level property textures")
		return false
	if model_metadata.property_texture_bytes <= 0:
		_fail("Property texture image bytes were not retained or accounted")
		return false
	if model_metadata.retained_memory_bytes <= 0:
		_fail("Model metadata retained-memory accounting was empty")
		return false
	var property_texture := (
		model_metadata.get_property_texture(0) as CesiumPropertyTexture
	)
	if (
		property_texture == null or not property_texture.is_valid() or
		property_texture.texture_name != "Surface metadata" or
		property_texture.class_name != "textureClass"
	):
		_fail("Model property texture identity or status was not preserved")
		return false
	var invalid_property_texture := (
		model_metadata.get_property_texture(1) as CesiumPropertyTexture
	)
	if (
		invalid_property_texture == null or
		invalid_property_texture.status_name !=
			"error_invalid_property_texture_class"
	):
		_fail("Invalid property texture class status was not preserved")
		return false
	if property_texture.get_property_names() != [
		"category", "default_texel", "unsupported_matrix"
	]:
		_fail(
			"Property texture definitions were incomplete or nondeterministic: %s"
			% [property_texture.get_property_names()]
		)
		return false
	var category := (
		property_texture.find_property("category")
		as CesiumPropertyTextureProperty
	)
	if (
		category == null or not category.is_valid() or
		category.value_type != "SCALAR" or
		category.component_type != "UINT8" or not category.normalized or
		category.texture_coordinate_set_index != 0 or category.channels != [0]
	):
		_fail("Property texture scalar definition was not preserved")
		return false
	if category.get_sampler_details().get("wrap_s", -1) != 33071:
		_fail("Property texture sampler wrap mode was not preserved")
		return false
	var texture_transform := category.get_texture_transform_details()
	if texture_transform.get("offset", Vector2.ZERO) != Vector2(0.1, 0.0):
		_fail("Property texture KHR_texture_transform was not preserved")
		return false
	if category.get_raw_value(Vector2(0.45, 0.25)) != 1:
		_fail("Property texture direct sampling returned the wrong raw texel")
		return false
	if not is_equal_approx(
		float(category.get_value(Vector2(0.45, 0.25))),
		5.0 + 2.0 / 255.0
	):
		_fail("Property texture normalization/override transforms were incorrect")
		return false
	var default_texel := (
		property_texture.find_property("default_texel")
		as CesiumPropertyTextureProperty
	)
	if (
		default_texel == null or
		default_texel.status_name != "empty_property_with_default" or
		default_texel.get_value(Vector2(123.0, -456.0)) != 42 or
		default_texel.get_raw_value(Vector2.ZERO) != null
	):
		_fail("Default-only property texture value was not preserved")
		return false
	var unsupported_texture_property := (
		property_texture.find_property("unsupported_matrix")
		as CesiumPropertyTextureProperty
	)
	if (
		unsupported_texture_property == null or
		unsupported_texture_property.status_name != "error_unsupported_property"
	):
		_fail("Unsupported property texture shape did not retain its status")
		return false
	var direct_texture_values := property_texture.get_metadata_values_for_uv(
		Vector2(0.45, 0.25)
	)
	if (
		direct_texture_values.size() != 2 or
		direct_texture_values.get("default_texel", -1) != 42
	):
		_fail("Property texture aggregate UV query returned invalid properties")
		return false
	var primitive_features := (
		loaded_tile.get_loaded_tile_primitive(0).primitive_features
		as CesiumPrimitiveFeatures
	)
	var loaded_primitive := loaded_tile.get_loaded_tile_primitive(0)
	var primitive_metadata := (
		loaded_primitive.primitive_metadata as CesiumPrimitiveMetadata
	)
	if primitive_metadata == null:
		_fail("Loaded primitive did not expose EXT_structural_metadata")
		return false
	if primitive_metadata.property_texture_indices != [0, 1, 99]:
		_fail("Primitive property texture references were not preserved")
		return false
	if primitive_metadata.property_attribute_indices != [0, 1, 99]:
		_fail("Primitive property attribute references were not preserved")
		return false
	if primitive_metadata.get_property_attribute_count() != 2:
		_fail("Valid/invalid property attributes were not realized as expected")
		return false
	if primitive_metadata.retained_memory_bytes <= 0:
		_fail("Primitive metadata retained-memory accounting was empty")
		return false
	var property_attribute := (
		primitive_metadata.get_property_attribute(0) as CesiumPropertyAttribute
	)
	if (
		property_attribute == null or not property_attribute.is_valid() or
		property_attribute.attribute_name != "Vertex metadata" or
		property_attribute.class_name != "vertexClass" or
		property_attribute.element_count != 3
	):
		_fail("Property attribute identity, class, or size was not preserved")
		return false
	var invalid_property_attribute := (
		primitive_metadata.get_property_attribute(1) as CesiumPropertyAttribute
	)
	if (
		invalid_property_attribute == null or
		invalid_property_attribute.status_name !=
			"error_invalid_property_attribute_class"
	):
		_fail("Invalid property attribute class status was not preserved")
		return false
	if property_attribute.get_property_names() != [
		"default_vertex", "height", "unsupported_attribute"
	]:
		_fail("Property attribute definitions were incomplete or unordered")
		return false
	var height := (
		property_attribute.find_property("height")
		as CesiumPropertyAttributeProperty
	)
	if (
		height == null or height.get_raw_values() != [1.5, 2.5, 3.5] or
		height.get_values() != [13.0, 15.0, 17.0]
	):
		_fail("Property attribute raw/transformed vertex values were incorrect")
		return false
	var default_vertex := (
		property_attribute.find_property("default_vertex")
		as CesiumPropertyAttributeProperty
	)
	if (
		default_vertex == null or
		default_vertex.status_name != "empty_property_with_default" or
		default_vertex.get_values() != [7, 7, 7]
	):
		_fail("Default-only property attribute value was not preserved")
		return false
	var unsupported_attribute := (
		property_attribute.find_property("unsupported_attribute")
		as CesiumPropertyAttributeProperty
	)
	if (
		unsupported_attribute == null or
		unsupported_attribute.status_name != "error_invalid_property_data"
	):
		_fail("Unsupported property attribute did not preserve its status")
		return false
	if property_attribute.get_metadata_values_at_index(1) != {
		"default_vertex": 7,
		"height": 15.0,
	}:
		_fail("Property attribute vertex query returned wrong or invalid values")
		return false
	if not property_attribute.get_metadata_values_at_index(99).is_empty():
		_fail("Out-of-range property attribute vertex query was not rejected")
		return false
	if primitive_features == null or primitive_features.get_feature_id_set_count() != 2:
		_fail("Implicit and texture feature ID sets were not exposed")
		return false
	var implicit_set := primitive_features.get_feature_id_set(0) as CesiumFeatureIdSet
	if implicit_set.feature_id_set_type_name != "implicit":
		_fail("Feature ID set without attribute or texture was not implicit")
		return false
	if implicit_set.get_feature_id_for_vertex(2) != 2:
		_fail("Implicit feature IDs did not follow the source vertex index")
		return false
	if primitive_features.get_feature_id_from_face(0) != 0:
		_fail("Implicit face feature lookup returned the wrong ID")
		return false
	var texture_set := primitive_features.get_feature_id_set(1) as CesiumFeatureIdSet
	if texture_set == null or texture_set.feature_id_set_type_name != "texture":
		_fail("Texture feature ID set was not exposed")
		return false
	if texture_set.get_feature_id_for_texture_coordinate(Vector2(0.75, 0.25)) != 1:
		_fail("Direct feature texture sampling returned the wrong texel")
		return false
	if texture_set.get_feature_id_for_texture_coordinate(Vector2(0.45, 0.25)) != 1:
		_fail("Feature texture sampling did not apply KHR_texture_transform")
		return false
	var arrays := loaded_tile.mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	if vertices.size() < 3:
		_fail("Texture feature fixture did not realize a triangle")
		return false
	var exact_local_hit := vertices[0] * 0.1 + vertices[1] * 0.8 + vertices[2] * 0.1
	if primitive_features.get_feature_id_from_face(0, 1) != 0:
		_fail("Texture face fallback did not sample its first vertex")
		return false
	var exact_feature_id := primitive_features.get_feature_id_from_hit(
		0,
		exact_local_hit,
		1
	)
	if exact_feature_id != 1:
		_fail(
			"Exact texture hit did not interpolate UVs before sampling: %d"
			% exact_feature_id
		)
		return false
	var exact_texture_values := (
		primitive_metadata.get_property_texture_values_from_hit(
			model_metadata,
			0,
			0,
			exact_local_hit
		)
	)
	if (
		exact_texture_values.get("default_texel", -1) != 42 or
		not is_equal_approx(
			float(exact_texture_values.get("category", -1.0)),
			5.0 + 2.0 / 255.0
		)
	):
		_fail("Exact property texture hit did not interpolate and sample UVs")
		return false
	if not primitive_metadata.get_property_texture_values_from_hit(
		model_metadata,
		99,
		0,
		exact_local_hit
	).is_empty():
		_fail("Invalid primitive property texture reference was not rejected")
		return false
	if not primitive_metadata.get_property_texture_values_from_hit(
		model_metadata,
		1,
		0,
		exact_local_hit
	).is_empty():
		_fail("Invalid referenced property texture was not rejected")
		return false
	if (
		primitive_features.get_metadata_values_from_hit(
			loaded_tile.model_metadata,
			0,
			exact_local_hit,
			1
		).get("default_only", -1) != 10
	):
		_fail("Exact texture hit did not join to its property-table row")
		return false

	# Exercise the public bridge with an actual Godot physics ray result. The
	# collision body is a child of the streamed tile, and the utility resolves
	# the global collision face back to its source surface and local face.
	await physics_frame
	var edge_a: Vector3 = vertices[1] - vertices[0]
	var edge_b: Vector3 = vertices[2] - vertices[0]
	var normal := edge_a.cross(edge_b).normalized()
	var world_hit := loaded_tile.to_global(exact_local_hit)
	var world_normal := (loaded_tile.global_basis * normal).normalized()
	var query := PhysicsRayQueryParameters3D.create(
		world_hit + world_normal * 10.0,
		world_hit - world_normal * 10.0
	)
	var ray_hit := test_root.get_world_3d().direct_space_state.intersect_ray(query)
	if ray_hit.is_empty():
		query = PhysicsRayQueryParameters3D.create(
			world_hit - world_normal * 10.0,
			world_hit + world_normal * 10.0
		)
		ray_hit = test_root.get_world_3d().direct_space_state.intersect_ray(query)
	if ray_hit.is_empty():
		_fail("Godot physics ray did not hit streamed Cesium collision")
		return false
	var resolved_hit := CesiumMetadataPicking.resolve_hit(ray_hit, 1)
	if not resolved_hit.get("valid", false):
		_fail("Cesium metadata picking did not resolve physics hit: %s" % [resolved_hit])
		return false
	if (
		resolved_hit.get("surface_index", -1) != 0 or
		resolved_hit.get("surface_face_index", -1) != 0 or
		resolved_hit.get("feature_id", -1) != 1
	):
		_fail("Physics hit resolved to the wrong surface, face, or feature")
		return false
	if resolved_hit.get("metadata", {}).get("default_only", -1) != 10:
		_fail("Physics hit did not return feature metadata")
		return false
	if resolved_hit.get("primitive") != loaded_tile.get_loaded_tile_primitive(0):
		_fail("Physics hit did not retain the loaded primitive context")
		return false
	if resolved_hit.get("primitive_metadata") != primitive_metadata:
		_fail("Physics hit did not expose primitive structural metadata")
		return false
	var picked_texture_values := (
		CesiumMetadataPicking.get_property_texture_values_from_hit(ray_hit, 0)
	)
	if (
		picked_texture_values.get("default_texel", -1) != 42 or
		not is_equal_approx(
			float(picked_texture_values.get("category", -1.0)),
			5.0 + 2.0 / 255.0
		)
	):
		_fail("Physics hit did not resolve exact property texture metadata")
		return false
	if CesiumMetadataPicking.resolve_hit({}).get("error", "") != "empty_hit":
		_fail("Metadata picking did not reject an empty physics hit deterministically")
		return false
	var default_property := table.find_property("default_only") as CesiumMetadataProperty
	if default_property == null:
		_fail("Missing schema property backed only by a default")
		return false
	if default_property.status_name != "empty_property_with_default":
		_fail("Default-only property did not preserve its status")
		return false
	if default_property.size != 3 or default_property.get_values() != [10, 10, 10]:
		_fail("Default-only property did not apply its value to every feature")
		return false
	if default_property.get_raw_value(0) != null:
		_fail("Default-only property invented an encoded raw value")
		return false

	var transformed := table.find_property("transformed") as CesiumMetadataProperty
	if transformed == null or transformed.get_raw_values() != [0, 127, 255]:
		_fail("Transformed metadata property lost its raw encoded values")
		return false
	if not is_equal_approx(float(transformed.get_value(0)), 2.0):
		_fail("Metadata normalization/scale/offset was not applied")
		return false
	if not is_equal_approx(float(transformed.get_value(2)), 99.0):
		_fail("Metadata no-data value did not resolve to the schema default")
		return false
	var details := transformed.get_value_transform_details()
	var expected_details := {
		"offset": 2.0,
		"scale": 4.0,
		"minimum": 2.0,
		"maximum": 6.0,
		"no_data": 255,
		"default": 99.0,
	}
	if details != expected_details:
		_fail("Metadata transform details were incomplete: %s" % [details])
		return false

	var retained_features := primitive_features
	var retained_metadata := loaded_tile.model_metadata
	var retained_primitive_metadata := primitive_metadata
	var retained_property_attribute := property_attribute
	var retained_property_texture := property_texture
	var retained_category := category
	tileset.free()
	await process_frame
	if retained_features.get_feature_id_from_hit(0, exact_local_hit, 1) != 1:
		_fail("Exact texture pixels or UVs did not survive source-tile destruction")
		return false
	if retained_property_attribute.get_metadata_values_at_index(2).get(
		"height",
		-1.0
	) != 17.0:
		_fail("Property attribute values did not survive tile destruction")
		return false
	if retained_category.get_raw_value(Vector2(0.45, 0.25)) != 1:
		_fail("Property texture image did not survive tile destruction")
		return false
	if retained_property_texture.get_metadata_values_for_uv(
		Vector2(0.45, 0.25)
	).get("default_texel", -1) != 42:
		_fail("Property texture Resource did not survive tile destruction")
		return false
	if retained_primitive_metadata.get_property_texture_values_from_hit(
		retained_metadata,
		0,
		0,
		exact_local_hit
	).get("default_texel", -1) != 42:
		_fail("Exact property texture query did not survive tile destruction")
		return false
	if (
		retained_features.get_metadata_values_from_hit(
			retained_metadata,
			0,
			exact_local_hit,
			1
		).get("default_only", -1) != 10
	):
		_fail("Exact texture metadata did not survive source-tile destruction")
		return false
	georeference.free()
	receiver.free()
	await process_frame
	return true


func _check_property_shape(
	property: CesiumMetadataProperty,
	value_type: String,
	component_type: String,
	is_array: bool,
	normalized: bool,
	fixed_count: int
) -> bool:
	if property == null or not property.is_valid():
		_fail("Missing or invalid metadata property %s" % value_type)
		return false
	if property.value_type != value_type or property.component_type != component_type:
		_fail(
			"Metadata property type mismatch: expected %s/%s, received %s/%s"
			% [value_type, component_type, property.value_type, property.component_type]
		)
		return false
	if property.is_array != is_array or property.normalized != normalized:
		_fail("Metadata property array/normalization flags were not preserved")
		return false
	if property.fixed_array_count != fixed_count:
		_fail("Metadata property fixed array count was not preserved")
		return false
	return true


func _array_is_approximately(actual: Array, expected: Array) -> bool:
	if actual.size() != expected.size():
		return false
	for index in range(actual.size()):
		if not is_equal_approx(float(actual[index]), float(expected[index])):
			return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
