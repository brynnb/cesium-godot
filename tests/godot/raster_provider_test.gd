extends SceneTree

const RecordingReceiver := preload("res://recording_lifecycle_receiver.gd")
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 600

var server_url := ""
var request_marker := ""
var test_root: Node3D
var camera: Camera3D
var receiver: Cesium3DTilesetLifecycleEventReceiver
var tileset: Cesium3DTileset


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	server_url = OS.get_environment("CESIUM_TEST_SERVER_URL")
	request_marker = OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	if server_url.is_empty() or request_marker.is_empty():
		_fail("Raster provider test server environment was not provided")
		return

	if not _check_provider_configuration_api():
		return
	if not await _check_real_provider_and_generation_recreation():
		return
	if not await _check_commercial_provider_lifecycle():
		return

	print("Cesium raster provider and generation-recreation test passed")
	_release_test_references()
	test_root.free()
	test_root = null
	await process_frame
	quit(0)


func _check_provider_configuration_api() -> bool:
	var ion := CesiumIonRasterOverlay.new()
	ion.key = "IonImagery"
	ion.asset_id = 1234
	ion.ion_access_token = "must-not-appear-in-diagnostics"
	var ion_config: Dictionary = ion.get_configuration()
	if (
		ion.get_provider_type() != "cesium_ion" or
		ion_config.material_key != "IonImagery" or
		ion_config.asset_id != 1234 or
		ion_config.access_token_source != "overlay_override" or
		str(ion_config).find("must-not-appear-in-diagnostics") >= 0
	):
		_fail("Cesium ion provider diagnostics were incomplete or exposed a token")
		return false
	ion.free()

	var bing := CesiumBingMapsRasterOverlay.new()
	bing.api_key = "must-not-appear-in-diagnostics"
	bing.map_style = 99
	bing.culture = "fr-FR"
	bing.service_url = "https://example.invalid/bing"
	var bing_config: Dictionary = bing.get_configuration()
	if (
		bing.get_provider_type() != "bing_maps" or
		bing.map_style != CesiumBingMapsRasterOverlay.CollinsBart or
		bing_config.map_style_name != "CollinsBart" or
		bing_config.api_key_configured != true or
		str(bing_config).find("must-not-appear-in-diagnostics") >= 0
	):
		_fail("Bing provider configuration was incomplete or exposed its key")
		return false
	bing.free()

	var google := CesiumGoogleMapTilesRasterOverlay.new()
	google.api_key = "must-not-appear-in-diagnostics"
	google.map_type = CesiumGoogleMapTilesRasterOverlay.Roadmap
	google.image_format = CesiumGoogleMapTilesRasterOverlay.Png
	google.scale = CesiumGoogleMapTilesRasterOverlay.Scale4x
	google.layer_types = 255
	google.styles = PackedStringArray(['{"featureType":"road"}'])
	var google_config: Dictionary = google.get_configuration()
	if (
		google.get_provider_type() != "google_map_tiles" or
		google.layer_types != 7 or
		google_config.map_type_name != "roadmap" or
		google_config.scale_name != "scaleFactor4x" or
		google_config.style_count != 1 or
		google_config.api_key_configured != true or
		str(google_config).find("must-not-appear-in-diagnostics") >= 0
	):
		_fail("Google provider configuration was incomplete or exposed its key")
		return false
	google.free()

	var tms := CesiumTileMapServiceRasterOverlay.new()
	tms.url = "https://example.invalid/tms/"
	tms.specify_zoom_levels = true
	tms.minimum_level = -2
	tms.maximum_level = 9
	if (
		tms.get_provider_type() != "tile_map_service" or
		tms.minimum_level != 0 or tms.maximum_level != 9
	):
		_fail("TMS provider did not preserve its typed, bounded configuration")
		return false
	tms.free()

	var template := CesiumUrlTemplateRasterOverlay.new()
	template.template_url = "https://{s}.example.invalid/{z}/{x}/{y}.png"
	template.projection = CesiumUrlTemplateRasterOverlay.ProjectionGeographic
	template.specify_tiling_scheme = true
	template.root_tiles_x = 0
	template.rectangle_south = -120.0
	if (
		template.get_provider_type() != "url_template" or
		template.root_tiles_x != 1 or
		template.rectangle_south != -90.0 or
		template.get_configuration().projection_name != "geographic"
	):
		_fail("URL-template projection or tiling-scheme bounds were incorrect")
		return false
	template.free()

	var headers := {"Authorization": "configuration-copy"}
	var wms := CesiumWebMapServiceRasterOverlay.new()
	wms.base_url = "https://example.invalid/wms"
	wms.layers = "base,labels"
	wms.version = "1.1.1"
	wms.format = "image/jpeg"
	wms.tile_width = 8
	wms.tile_height = 9000
	wms.minimum_level = 5
	wms.maximum_level = 3
	wms.request_headers = headers
	headers.Authorization = "mutated-source"
	var returned_headers: Dictionary = wms.request_headers
	returned_headers.Authorization = "mutated-return"
	var wms_config: Dictionary = wms.get_configuration()
	if (
		wms.get_provider_type() != "web_map_service" or
		wms.tile_width != 64 or wms.tile_height != 2048 or
		wms_config.maximum_level != 5 or
		wms_config.request_headers.Authorization != "configuration-copy"
	):
		_fail("WMS options were not normalized or defensively copied: %s" % [wms_config])
		return false
	wms.free()

	var wmts := CesiumWebMapTileServiceRasterOverlay.new()
	wmts.base_url = "https://{s}.example.invalid/{TileMatrix}/{TileCol}/{TileRow}.png"
	wmts.layer = "world"
	wmts.style = "default"
	wmts.tile_matrix_set_id = "EPSG:4326"
	wmts.tile_matrix_set_label_prefix = "EPSG:4326:"
	wmts.projection = CesiumWebMapTileServiceRasterOverlay.ProjectionGeographic
	wmts.specify_tiling_scheme = true
	wmts.root_tiles_x = -4
	wmts.root_tiles_y = 2
	wmts.specify_zoom_levels = true
	wmts.minimum_level = 2
	wmts.maximum_level = 6
	wmts.subdomains = PackedStringArray(["a", "b"])
	var dimensions := {"Time": "2026-08-12"}
	wmts.dimensions = dimensions
	dimensions.Time = "mutated"
	wmts.request_headers = {"X-Provider": "wmts"}
	var wmts_config: Dictionary = wmts.get_configuration()
	if (
		wmts.get_provider_type() != "web_map_tile_service" or
		wmts_config.projection_name != "geographic" or
		wmts.root_tiles_x != 1 or wmts.root_tiles_y != 2 or
		wmts_config.subdomains != PackedStringArray(["a", "b"]) or
		wmts_config.dimensions.Time != "2026-08-12"
	):
		_fail("WMTS provider lost projection, tiling, labels, or Native options: %s" % [wmts_config])
		return false
	wmts.free()
	return true


func _check_real_provider_and_generation_recreation() -> bool:
	test_root = Node3D.new()
	test_root.name = "RasterProviderTest"
	root.add_child(test_root)

	receiver = RecordingReceiver.new()
	test_root.add_child(receiver)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = server_url + "/raster-tileset.json"
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver

	georeference.add_child(tileset)
	if not await _pump_until_event("visibility:true"):
		_fail("Base fixture did not become visible before adding imagery")
		return false

	var overlay := CesiumUrlTemplateRasterOverlay.new()
	overlay.name = "TestUrlTemplateOverlay"
	overlay.key = "OverlayA"
	overlay.template_url = server_url + "/raster/{z}/{x}/{y}.png"
	overlay.minimum_level = 0
	overlay.maximum_level = 2
	overlay.maximum_simultaneous_tile_loads = 2
	overlay.request_headers = {"X-Cesium-Overlay-Test": "generation-safe"}
	tileset.add_child(overlay)

	if not await _pump_until_attachment_count(1):
		_fail("URL-template imagery did not attach to real streamed geometry: overlay=%s statistics=%s events=%s" % [overlay.get_configuration(), tileset.get_streaming_statistics(), receiver.events])
		return false
	if not overlay.is_added_to_tileset():
		_fail("Provider did not report its first Native tileset attachment")
		return false
	if not _marker_contains("generation-safe"):
		_fail("Raster request did not use the provider's custom HTTP header")
		return false
	if not _latest_attachment_is_valid(0):
		return false

	var snapshot_count_before_recreation: int = receiver.overlay_snapshots.size()
	var attachment_count_before_recreation := _event_count("raster_attached:OverlayA")
	var previous_worker_count := int(tileset.worker_thread_count)
	tileset.worker_thread_count = 1 if previous_worker_count != 1 else 2
	if overlay.is_added_to_tileset():
		_fail("Provider retained a stale Native instance during tileset recreation")
		return false
	if not await _pump_until_attachment_count(attachment_count_before_recreation + 1):
		_fail("URL-template imagery did not reattach after tileset recreation")
		return false
	if (
		not overlay.is_added_to_tileset() or
		not _latest_attachment_is_valid(snapshot_count_before_recreation)
	):
		_fail("Recreated tileset did not own a fresh, usable provider instance")
		return false
	if not await _check_local_url_template(overlay):
		return false

	var drain_deadline := Time.get_ticks_msec() + 1000
	while Time.get_ticks_msec() < drain_deadline:
		if int(tileset.get_streaming_statistics().get("requests_in_flight", 0)) == 0:
			break
		tileset.update_tileset(camera.global_transform)
		await process_frame
	return true


func _check_local_url_template(remote_overlay: CesiumUrlTemplateRasterOverlay) -> bool:
	remote_overlay.free()
	await process_frame

	var raster_directory := ProjectSettings.globalize_path("user://raster/0/0")
	if DirAccess.make_dir_recursive_absolute(raster_directory) != OK:
		_fail("Could not create the generated local-raster fixture directory")
		return false
	var png_bytes := Marshalls.base64_to_raw(
		"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
	)
	var png_file := FileAccess.open(raster_directory + "/0.png", FileAccess.WRITE)
	if png_file == null:
		_fail("Could not write the generated local-raster fixture")
		return false
	png_file.store_buffer(png_bytes)
	png_file.close()

	var local_overlay := CesiumUrlTemplateRasterOverlay.new()
	local_overlay.name = "TestLocalTemplateOverlay"
	local_overlay.key = "OverlayB"
	local_overlay.template_url = (
		"file://" +
		ProjectSettings.globalize_path("user://raster/{z}/{x}/{y}.png")
	)
	local_overlay.minimum_level = 0
	local_overlay.maximum_level = 0
	var previous_count := _event_count("raster_attached:OverlayB")
	tileset.add_child(local_overlay)
	if not await _pump_until_named_attachment("OverlayB", previous_count + 1):
		_fail("Local file URL-template imagery did not stream and attach")
		return false
	if not local_overlay.is_added_to_tileset():
		_fail("Local URL-template provider lost its Native attachment")
		return false
	local_overlay.free()
	await process_frame
	return true


func _check_commercial_provider_lifecycle() -> bool:
	var bing := CesiumBingMapsRasterOverlay.new()
	bing.name = "TestBingOverlay"
	bing.key = "BingOverlay"
	bing.api_key = "test-bing-key"
	bing.map_style = CesiumBingMapsRasterOverlay.AerialWithLabelsOnDemand
	bing.culture = "fr-FR"
	bing.service_url = server_url
	tileset.add_child(bing)
	if not await _pump_until_named_attachment("BingOverlay", 1):
		_fail("Bing provider did not attach through its metadata and tile flow")
		return false
	if (
		not _marker_contains("bing-metadata:AerialWithLabelsOnDemand:test-bing-key:fr-FR") or
		not _marker_contains("bing-tile:")
	):
		_fail("Bing provider did not preserve style, key, culture, and tile requests")
		return false
	bing.free()
	await process_frame

	var google := CesiumGoogleMapTilesRasterOverlay.new()
	google.name = "TestGoogleOverlay"
	google.key = "GoogleOverlay"
	google.api_key = "test-google-key"
	google.map_type = CesiumGoogleMapTilesRasterOverlay.Roadmap
	google.language = "fr-FR"
	google.region = "CA"
	google.image_format = CesiumGoogleMapTilesRasterOverlay.Png
	google.scale = CesiumGoogleMapTilesRasterOverlay.Scale2x
	google.high_dpi = true
	google.layer_types = (
		CesiumGoogleMapTilesRasterOverlay.LayerRoadmap |
		CesiumGoogleMapTilesRasterOverlay.LayerTraffic
	)
	google.styles = PackedStringArray([
		'{"featureType":"road","stylers":[{"color":"#ffffff"}]}',
		'{ deliberately malformed',
	])
	google.overlay = true
	google.api_base_url = server_url + "/google/"
	# The provider itself supports level 28, but this tiny 200 m fixture only
	# needs one coarse service tile to prove the complete session/imagery path.
	google.maximum_screen_space_error = 1.0e9
	google.maximum_texture_size = 1
	google.maximum_simultaneous_tile_loads = 2
	tileset.add_child(google)
	if not await _pump_until_named_attachment("GoogleOverlay", 1):
		_fail("Google Map Tiles provider did not create a session and attach imagery")
		return false
	if google.last_style_errors.size() != 1:
		_fail("Google styles did not retain one malformed-style diagnostic: %s" % [google.last_style_errors])
		return false
	var google_config: Dictionary = google.get_configuration()
	if (
		google_config.style_error_count != 1 or
		str(google_config).find("test-google-key") >= 0 or
		not _marker_contains("google-session:test-google-key:") or
		not _marker_contains('"highDpi": true') or
		not _marker_contains('"mapType": "roadmap"') or
		not _marker_contains("google-viewport:deterministic-google-session:test-google-key") or
		not _marker_contains("google-tile:")
	):
		_fail("Google session, styles, credentials, viewport, or tile flow was incomplete")
		return false
	google.free()
	await process_frame
	return true


func _release_test_references() -> void:
	if receiver != null and is_instance_valid(receiver):
		receiver.overlay_snapshots.clear()
		receiver.primitive_snapshots.clear()
		receiver.primitive_snapshot.clear()
		receiver.events.clear()
	receiver = null
	tileset = null


func _pump_until_attachment_count(required_count: int) -> bool:
	return await _pump_until_named_attachment("OverlayA", required_count)


func _pump_until_named_attachment(key: String, required_count: int) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if _event_count("raster_attached:%s" % key) >= required_count:
			return true
		await process_frame
	return false


func _pump_until_event(event_name: String) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if receiver.events.has(event_name):
			return true
		await process_frame
	return false


func _event_count(event_name: String) -> int:
	var count := 0
	for event in receiver.events:
		if event == event_name:
			count += 1
	return count


func _latest_attachment_is_valid(first_index: int) -> bool:
	var snapshot: Dictionary = {}
	for index in range(receiver.overlay_snapshots.size() - 1, first_index - 1, -1):
		var candidate: Dictionary = receiver.overlay_snapshots[index]
		if candidate.get("event", "") == "attached":
			snapshot = candidate
			break
	if snapshot.is_empty():
		_fail("Raster lifecycle callback did not include a typed binding snapshot")
		return false
	if (
		snapshot.key != "OverlayA" or
		not snapshot.texture_valid or not snapshot.material_valid or
		not snapshot.primitive_valid or not snapshot.attached or
		not snapshot.shader_texture_valid
	):
		_fail("Raster binding was incomplete: %s" % [snapshot])
		return false
	return true


func _marker_contains(expected: String) -> bool:
	if not FileAccess.file_exists(request_marker):
		return false
	var file := FileAccess.open(request_marker, FileAccess.READ)
	if file == null:
		return false
	var contents := file.get_as_text()
	return contents.find(expected) >= 0


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
