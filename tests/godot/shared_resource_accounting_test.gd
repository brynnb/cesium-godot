extends SceneTree

const SharedResourceReceiver := preload("res://shared_resource_receiver.gd")
const LOAD_TIMEOUT_FRAMES := 600
const UNLOAD_TIMEOUT_FRAMES := 300
const EARTH_RADIUS := 6_378_137.0

var test_root: Node3D
var camera: Camera3D
var tileset: Cesium3DTileset
var receiver: Cesium3DTilesetLifecycleEventReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "SharedResourceAccountingTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 350.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = SharedResourceReceiver.new()
	test_root.add_child(receiver)
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/shared_resources/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if (
			receiver.loaded_tiles.size() >= 2 and
			receiver.textures.size() >= 2 and receiver.shaders.size() >= 2
		):
			break
		await process_frame
	if (
		receiver.loaded_tiles.size() != 2 or receiver.textures.size() != 2 or
		receiver.shaders.size() != 2
	):
		_fail("Two shared-image/shader tiles did not realize")
		return
	if receiver.textures[0] != receiver.textures[1]:
		_fail("Tiles referencing one Native ImageAsset created two Godot textures")
		return
	if receiver.shaders[0] != receiver.shaders[1]:
		_fail("Equivalent cross-tile glTF variants compiled two Godot shaders")
		return

	var stats := tileset.get_streaming_statistics()
	if (
		int(stats.get("shared_texture_cache_misses", -1)) != 1 or
		int(stats.get("shared_texture_cache_hits", -1)) != 1 or
		int(stats.get("shared_texture_live_count", -1)) != 1 or
		int(stats.get("shared_texture_live_bytes", -1)) != 16 or
		int(stats.get("shared_texture_maximum_live_count", -1)) != 1 or
		int(stats.get("shared_texture_maximum_live_bytes", -1)) != 16 or
		int(stats.get("released_cpu_texture_count", -1)) != 1 or
		int(stats.get("released_cpu_texture_bytes", -1)) != 16
	):
		_fail("Shared texture diagnostics were not de-duplicated: %s" % [stats])
		return
	if (
		int(stats.get("shared_shader_cache_misses", -1)) != 1 or
		int(stats.get("shared_shader_cache_hits", -1)) != 1 or
		int(stats.get("shared_shader_cache_entries", -1)) != 1 or
		int(stats.get("shared_shader_cache_maximum_entries", -1)) != 1
	):
		_fail("Equivalent shader variants were not shared: %s" % [stats])
		return
	# Two 66-byte glTF buffers plus one decoded 2x2 RGBA image. Before Native's
	# shared-image accounting this was 164 bytes because the image was charged
	# to both tiles.
	if int(stats.get("loaded_data_bytes", -1)) != 148:
		_fail("Native tile-cache bytes double-counted the shared image: %s" % [stats])
		return
	# Do not let the test observer itself retain the Texture2D while checking the
	# renderer's final-owner release below.
	receiver.textures.clear()
	receiver.shaders.clear()

	# Make both now-hidden leaves immediately eligible for eviction. The Godot
	# texture remains attached to Native's small inactive-image depot so a tile
	# that returns can reuse it even though its CPU upload pixels are gone.
	tileset.maximum_cached_bytes = 0
	camera.position = Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
	camera.look_at(Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0), Vector3.UP)
	for _frame in range(UNLOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		stats = tileset.get_streaming_statistics()
		if receiver.unload_count >= 2:
			break
	if (
		receiver.unload_count != 2 or
		int(stats.get("shared_texture_live_count", -1)) != 1 or
		int(stats.get("shared_texture_live_bytes", -1)) != 16 or
		int(stats.get("released_cpu_texture_bytes", -1)) != 16
	):
		_fail("Inactive Native image did not retain only its uploaded texture: %s" % [stats])
		return

	# Independently decoded embedded and external images with identical pixels
	# must also share one renderer texture. This is the path used by authored
	# EXT_mesh_gpu_instancing wrappers around one shared binary object model.
	tileset.free()
	tileset = null
	await process_frame
	receiver.loaded_tiles.clear()
	receiver.textures.clear()
	receiver.shaders.clear()
	receiver.materials.clear()
	receiver.customized_materials.clear()
	receiver.meshes.clear()
	receiver.unload_count = 0
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 350.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/shared_resources/content_identical_tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.loaded_tiles.size() >= 2 and receiver.textures.size() >= 2:
			break
		await process_frame
	if receiver.loaded_tiles.size() != 2 or receiver.textures.size() != 2:
		_fail("Two content-identical image tiles did not realize")
		return
	if receiver.textures[0] != receiver.textures[1]:
		_fail("Content-identical ImageAssets created two Godot textures: %s" % [
			tileset.get_streaming_statistics()
		])
		return
	stats = tileset.get_streaming_statistics()
	if (
		int(stats.get("shared_texture_cache_misses", -1)) != 0 or
		int(stats.get("shared_texture_cache_hits", -1)) != 2 or
		int(stats.get("shared_texture_live_count", -1)) != 0 or
		int(stats.get("shared_texture_live_bytes", -1)) != 0 or
		int(stats.get("released_cpu_texture_count", -1)) != 1 or
		int(stats.get("released_cpu_texture_bytes", -1)) != 16
	):
		_fail("Native image-depot texture reuse was not accounted: %s" % [stats])
		return

	# The same resolved model URL may appear under different raw tile IDs and
	# transforms. It must share immutable Godot renderer resources without
	# collapsing the two independently placed scene nodes.
	tileset.free()
	tileset = null
	await process_frame
	receiver.loaded_tiles.clear()
	receiver.textures.clear()
	receiver.shaders.clear()
	receiver.materials.clear()
	receiver.customized_materials.clear()
	receiver.meshes.clear()
	receiver.unload_count = 0
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 350.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/shared_resources/repeated_model_tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.loaded_tiles.size() >= 2 and receiver.meshes.size() >= 2:
			break
		await process_frame
	if (
		receiver.loaded_tiles.size() != 2 or receiver.meshes.size() != 2 or
		receiver.materials.size() != 2 or
		receiver.customized_materials.size() != 2
	):
		_fail("Two repeated-model placements did not realize")
		return
	if receiver.loaded_tiles[0] == receiver.loaded_tiles[1]:
		_fail("Repeated model content collapsed two placement nodes")
		return
	if receiver.meshes[0] != receiver.meshes[1]:
		_fail("One resolved model URL created two Godot meshes")
		return
	if receiver.materials[0] != receiver.materials[1]:
		_fail("One resolved model URL created two immutable base materials")
		return
	if (
		receiver.customized_materials[0] == receiver.customized_materials[1] or
		receiver.customized_materials[0].get_meta("test_tile_instance_id") ==
			receiver.customized_materials[1].get_meta("test_tile_instance_id")
	):
		_fail("Per-tile customization mutated one shared material instance")
		return
	stats = tileset.get_streaming_statistics()
	if (
		int(stats.get("shared_model_cache_misses", -1)) != 1 or
		int(stats.get("shared_model_cache_hits", -1)) != 1 or
		int(stats.get("shared_model_live_count", -1)) != 1 or
		int(stats.get("shared_model_live_geometry_bytes", -1)) <= 0 or
		int(stats.get("shared_model_live_texture_bytes", -1)) != 16 or
		int(stats.get("shared_model_maximum_live_count", -1)) != 1
	):
		_fail("Repeated model renderer resources were not shared: %s" % [stats])
		return

	receiver.loaded_tiles.clear()
	receiver.textures.clear()
	receiver.shaders.clear()
	receiver.materials.clear()
	receiver.customized_materials.clear()
	receiver.meshes.clear()
	tileset.maximum_cached_bytes = 0
	camera.position = Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
	camera.look_at(Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0), Vector3.UP)
	for _frame in range(UNLOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		stats = tileset.get_streaming_statistics()
		if (
			receiver.unload_count >= 2 and
			int(stats.get("shared_model_live_count", -1)) == 0
		):
			break
	if (
		receiver.unload_count != 2 or
		int(stats.get("shared_model_live_count", -1)) != 0 or
		int(stats.get("shared_model_live_geometry_bytes", -1)) != 0 or
		int(stats.get("shared_model_live_texture_bytes", -1)) != 0
	):
		_fail("Shared model survived its final tile lease: %s" % [stats])
		return

	print("Cesium shared-resource accounting test passed")
	test_root.free()
	quit(0)


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
