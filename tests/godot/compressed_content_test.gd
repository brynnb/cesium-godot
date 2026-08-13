extends SceneTree

const RecordingReceiver := preload("res://recording_lifecycle_receiver.gd")
const LOAD_TIMEOUT_FRAMES := 900
const EARTH_RADIUS := 6_378_137.0

var test_root: Node3D
var camera: Camera3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "CompressedContentTest"
	root.add_child(test_root)

	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 30.0, 30.0, 80.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS, 0.0, 0.0), Vector3.UP)

	var repository_root := ProjectSettings.globalize_path("res://../..").simplify_path()
	var fixtures: Array[Dictionary] = [
		{
			"name": "draco",
			"path": repository_root.path_join(
				"build/dependencies/sources/cesium-native/"
				+ "CesiumGltfReader/test/data/DracoCompressed/"
				+ "CesiumMilkTruck.gltf"
			),
			"minimum_surfaces": 1,
		},
		{
			"name": "meshopt",
			"path": repository_root.path_join(
				"build/dependencies/sources/cesium-native/"
				+ "CesiumGltfReader/test/data/DucksMeshopt/"
				+ "Duck-vp-12-vt-12-vn-12.glb"
			),
			"minimum_surfaces": 1,
		},
		{
			"name": "ktx2",
			"path": repository_root.path_join(
				"build/dependencies/sources/cesium-native/"
				+ "CesiumGltfReader/test/data/CesiumBalloon/"
				+ "CesiumBalloonKTX2Hacky.glb"
			),
			"minimum_surfaces": 5,
			"requires_texture": true,
		},
	]

	for fixture in fixtures:
		if not await _test_fixture(fixture):
			return

	print("Cesium Draco, meshopt, and KTX2 streaming tests passed")
	test_root.free()
	quit(0)


func _test_fixture(fixture: Dictionary) -> bool:
	var source_path := str(fixture.path)
	if not FileAccess.file_exists(source_path):
		_fail("Missing Cesium Native compressed fixture: %s" % source_path)
		return false

	var receiver := RecordingReceiver.new()
	receiver.name = "%sReceiver" % str(fixture.name).capitalize()
	test_root.add_child(receiver)

	var georeference := CesiumGeoreference.new()
	georeference.name = "%sCoordinates" % str(fixture.name).capitalize()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	var tileset := Cesium3DTileset.new()
	tileset.name = "%sTileset" % str(fixture.name).capitalize()
	tileset.data_source = 1
	tileset.url = "file://" + _write_fixture_tileset(
		str(fixture.name),
		source_path
	)
	tileset.create_physics_meshes = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	var visible := false
	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.has("visibility:true"):
			visible = true
			break
		await process_frame
	if not visible:
		_fail("Timed out streaming the %s fixture" % fixture.name)
		return false

	if receiver.primitive_snapshots.size() < int(fixture.minimum_surfaces):
		_fail(
			"%s decoded too few primitive surfaces: %d"
			% [fixture.name, receiver.primitive_snapshots.size()]
		)
		return false
	var loaded_tile := (
		receiver.primitive_snapshots[0].get("loaded_tile") as Cesium3DTile
	)
	if loaded_tile == null or loaded_tile.mesh == null:
		_fail("%s produced no realized Godot mesh" % fixture.name)
		return false
	var vertices: PackedVector3Array = loaded_tile.mesh.surface_get_arrays(0)[
		Mesh.ARRAY_VERTEX
	]
	if vertices.is_empty():
		_fail("%s decoded an empty vertex buffer" % fixture.name)
		return false

	if bool(fixture.get("requires_texture", false)):
		var texture: ImageTexture = null
		for snapshot in receiver.primitive_snapshots:
			texture = snapshot.get("default_base_texture") as ImageTexture
			if texture != null:
				break
		if texture == null:
			_fail("KTX2 content produced no Godot texture")
			return false
		if not texture.has_meta("cesium_compressed_pixel_format"):
			_fail("KTX2 texture lost its transcode diagnostics")
			return false
		if int(texture.get_meta("cesium_source_byte_size", 0)) <= 0:
			_fail("KTX2 texture reported an empty decoded/transcoded payload")
			return false

	tileset.free()
	await process_frame
	georeference.free()
	receiver.free()
	await process_frame
	return true


func _write_fixture_tileset(name: String, content_path: String) -> String:
	var output_directory := ProjectSettings.globalize_path(
		"res://.test-generated/compressed-content-fixtures"
	)
	DirAccess.make_dir_recursive_absolute(output_directory)
	var output_path := output_directory.path_join(name + "-tileset.json")
	var definition := {
		"asset": {"version": "1.1"},
		"geometricError": 0,
		"root": {
			"transform": [
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				EARTH_RADIUS, 0, 0, 1,
			],
			"boundingVolume": {
				"box": [
					0, 0, 0,
					100, 0, 0,
					0, 100, 0,
					0, 0, 100,
				]
			},
			"geometricError": 0,
			"refine": "REPLACE",
			"content": {"uri": "file://" + content_path},
		},
	}
	var output := FileAccess.open(output_path, FileAccess.WRITE)
	if output == null:
		return ""
	output.store_string(JSON.stringify(definition, "\t"))
	output.close()
	return output_path


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
