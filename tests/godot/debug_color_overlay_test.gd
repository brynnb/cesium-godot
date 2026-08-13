extends SceneTree

const DebugReceiver := preload("res://debug_color_overlay_receiver.gd")
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600

var test_root: Node3D
var camera: Camera3D
var tileset: Cesium3DTileset
var overlay: CesiumDebugColorizeTilesRasterOverlay
var receiver: Cesium3DTilesetLifecycleEventReceiver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	overlay = CesiumDebugColorizeTilesRasterOverlay.new()
	var configuration: Dictionary = overlay.get_configuration()
	if (
		overlay.get_provider_type() != "debug_colorize_tiles" or
		overlay.key != "Overlay0" or
		configuration.material_key != "Overlay0" or
		configuration.color_assignment != "random_per_geometry_tile" or
		int(configuration.image_width) != 1 or
		int(configuration.image_height) != 1 or
		int(configuration.opacity_byte) != 127 or
		not is_equal_approx(float(configuration.opacity), 127.0 / 255.0) or
		bool(configuration.requests_additional_detail)
	):
		_fail("Debug provider configuration did not match Cesium Native: %s" % [configuration])
		return

	test_root = Node3D.new()
	test_root.name = "DebugColorOverlayTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 350.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = DebugReceiver.new()
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
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	overlay.generate_mipmaps = false
	tileset.add_child(overlay)
	georeference.add_child(tileset)

	if not await _pump_until_attachment_count(2):
		_fail("Debug colors did not attach to both streamed fixture tiles")
		return
	if not overlay.is_added_to_tileset():
		_fail("Debug provider lost its Native tileset attachment")
		return
	if not _validate_attachment_range(0, 2):
		return

	var first_generation_bindings: Array = []
	for index in range(2):
		first_generation_bindings.append(receiver.attachments[index].binding)
	var first_generation_texture_ids := PackedInt64Array([
		int(receiver.attachments[0].texture_id),
		int(receiver.attachments[1].texture_id),
	])
	var detachments_before_refresh: int = receiver.detachments.size()
	overlay.refresh()
	if not await _pump_until_counts(4, detachments_before_refresh + 2):
		_fail("Refreshed debug provider did not replace both raster generations")
		return
	if not _validate_attachment_range(2, 4):
		return
	for binding in first_generation_bindings:
		if binding.attached or binding.texture != null or binding.material != null:
			_fail("A refreshed debug binding retained renderer resources")
			return
	for index in range(detachments_before_refresh, receiver.detachments.size()):
		var snapshot: Dictionary = receiver.detachments[index]
		if (
			not snapshot.attached_during_callback or
			not snapshot.texture_valid_during_callback or
			not snapshot.material_valid_during_callback
		):
			_fail("Debug detach callback ran after its resources were cleared: %s" % [snapshot])
			return
	var refreshed_texture_ids := PackedInt64Array([
		int(receiver.attachments[2].texture_id),
		int(receiver.attachments[3].texture_id),
	])
	if (
		first_generation_texture_ids[0] in refreshed_texture_ids or
		first_generation_texture_ids[1] in refreshed_texture_ids
	):
		_fail("Debug refresh reused a stale generation texture")
		return

	var active_bindings: Array = [
		receiver.attachments[2].binding,
		receiver.attachments[3].binding,
	]
	var detachments_before_removal: int = receiver.detachments.size()
	overlay.remove_from_tileset(tileset)
	if not await _pump_until_detachment_count(detachments_before_removal + 2):
		_fail("Removing the debug provider did not detach both tile colors")
		return
	if overlay.is_added_to_tileset():
		_fail("Removed debug provider still reports an active Native attachment")
		return
	for binding in active_bindings:
		if binding.attached or binding.texture != null or binding.material != null:
			_fail("Removed debug binding retained renderer resources")
			return

	print("Cesium debug color tile-overlay test passed")
	_release_references()
	test_root.free()
	test_root = null
	await process_frame
	quit(0)


func _validate_attachment_range(first: int, end: int) -> bool:
	var tile_ids: Dictionary = {}
	var texture_ids: Dictionary = {}
	for index in range(first, end):
		var snapshot: Dictionary = receiver.attachments[index]
		var pixel: Color = snapshot.pixel
		if (
			snapshot.overlay_key != "Overlay0" or
			int(snapshot.image_width) != 1 or int(snapshot.image_height) != 1 or
			not is_equal_approx(pixel.a, 127.0 / 255.0) or
			int(snapshot.coordinate_index) < 0 or
			not bool(snapshot.material_valid) or
			snapshot.material_default_key != "Overlay0" or
			int(snapshot.shader_texture_id) != int(snapshot.texture_id) or
			int(snapshot.shader_coordinate_index) != int(snapshot.coordinate_index) or
			str(snapshot.shader_code).find("overlay0_color") < 0 or
			str(snapshot.shader_code).find("base_color.rgb = mix") < 0
		):
			_fail("Debug texture or generated material binding was incomplete: %s" % [snapshot])
			return false
		tile_ids[snapshot.tile_id] = true
		texture_ids[snapshot.texture_id] = true
	if tile_ids.size() != end - first:
		_fail("Debug fixture did not color distinct geometry tiles: %s" % [tile_ids])
		return false
	if texture_ids.size() != end - first:
		_fail("Distinct geometry tiles shared one debug color texture: %s" % [texture_ids])
		return false
	return true


func _pump_until_attachment_count(required_count: int) -> bool:
	return await _pump_until_counts(required_count, 0)


func _pump_until_counts(required_attachments: int, required_detachments: int) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if (
			receiver.attachments.size() >= required_attachments and
			receiver.detachments.size() >= required_detachments
		):
			return true
		await process_frame
	return false


func _pump_until_detachment_count(required_count: int) -> bool:
	return await _pump_until_counts(0, required_count)


func _release_references() -> void:
	if receiver != null and is_instance_valid(receiver):
		receiver.attachments.clear()
		receiver.detachments.clear()
	receiver = null
	overlay = null
	tileset = null


func _fail(message: String) -> void:
	push_error(message)
	_release_references()
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
