extends SceneTree

const GeoJsonReceiver := preload("res://geojson_overlay_receiver.gd")
const FIXTURE := "res://fixtures/vector/overlay.geojson"
const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_FRAMES := 900

var server_url := ""
var request_marker := ""
var source_text := ""
var test_root: Node3D
var camera: Camera3D
var tileset: Cesium3DTileset
var receiver: Cesium3DTilesetLifecycleEventReceiver
var overlay: CesiumGeoJsonDocumentRasterOverlay
var loaded_documents: Array = []
var failures: Array[Dictionary] = []


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	server_url = OS.get_environment("CESIUM_TEST_SERVER_URL")
	request_marker = OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	if server_url.is_empty() or request_marker.is_empty():
		_fail("GeoJSON overlay test server environment was not provided")
		return
	var file := FileAccess.open(FIXTURE, FileAccess.READ)
	if file == null:
		_fail("Could not open GeoJSON overlay fixture")
		return
	source_text = file.get_as_text()
	file.close()

	if not _check_configuration_api():
		return
	if not await _check_rasterization_refresh_and_sources():
		return

	print("Cesium GeoJSON raster-overlay lifecycle test passed")
	_release_references()
	test_root.free()
	test_root = null
	await process_frame
	quit(0)


func _check_configuration_api() -> bool:
	var candidate := CesiumGeoJsonDocumentRasterOverlay.new()
	var headers := {"X-GeoJSON-Test": "original"}
	candidate.source = CesiumGeoJsonDocumentRasterOverlay.FromUrl
	candidate.url = "res://fixtures/vector/overlay.geojson"
	candidate.request_headers = headers
	headers["X-GeoJSON-Test"] = "mutated-source"
	var returned_headers: Dictionary = candidate.request_headers
	returned_headers["X-GeoJSON-Test"] = "mutated-return"
	candidate.mip_levels = 99
	candidate.ion_asset_id = -20
	candidate.ion_access_token = "must-not-appear-in-diagnostics"
	var configuration: Dictionary = candidate.get_configuration()
	if (
		candidate.get_provider_type() != "geo_json" or
		candidate.mip_levels != 8 or candidate.ion_asset_id != 0 or
		configuration.request_headers["X-GeoJSON-Test"] != "original" or
		configuration.ion_access_token_source != "overlay_override" or
		str(configuration).find("must-not-appear-in-diagnostics") >= 0
	):
		candidate.free()
		_fail("GeoJSON provider configuration was not bounded or token-safe: %s" % [configuration])
		return false
	candidate.free()
	return true


func _check_rasterization_refresh_and_sources() -> bool:
	test_root = Node3D.new()
	test_root.name = "GeoJsonOverlayTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 350.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)

	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	receiver = GeoJsonReceiver.new()
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
	georeference.add_child(tileset)
	if not await _pump_until_event("visibility:true"):
		_fail("Base geometry did not become visible before adding GeoJSON")
		return false

	var document := CesiumGeoJsonDocument.new()
	if not document.load_from_string(source_text):
		_fail("GeoJSON document fixture did not parse")
		return false
	var style := CesiumVectorStyle.new()
	_set_style_color(style, Color(1.0, 0.0, 0.0, 1.0))

	overlay = CesiumGeoJsonDocumentRasterOverlay.new()
	overlay.key = "Overlay0"
	overlay.source = CesiumGeoJsonDocumentRasterOverlay.FromDocument
	overlay.document = document
	overlay.default_style = style
	overlay.generate_mipmaps = false
	overlay.maximum_texture_size = 128
	overlay.document_loaded.connect(_on_document_loaded)
	overlay.document_load_failed.connect(_on_document_load_failed)
	# Exercise explicit attachment before parenting; providers still need the
	# owning tileset's async system while they construct their Native instance.
	if overlay.add_to_tileset(tileset) != OK:
		_fail("Explicit GeoJSON provider attachment failed")
		return false
	tileset.add_child(overlay)
	if not await _pump_until_counts(2, 0, 1):
		_fail("Parsed-document GeoJSON did not rasterize onto both tiles")
		return false
	if not _validate_attachment_range(0, 2, Color(1.0, 0.0, 0.0, 1.0)):
		return false
	if not overlay.is_added_to_tileset() or not failures.is_empty():
		_fail("Parsed-document provider lost attachment or reported failure")
		return false

	var first_bindings: Array = [
		receiver.attachments[0].binding,
		receiver.attachments[1].binding,
	]
	var first_texture_ids := PackedInt64Array([
		int(receiver.attachments[0].texture_id),
		int(receiver.attachments[1].texture_id),
	])
	_set_style_color(style, Color(0.0, 0.0, 1.0, 1.0))
	if not await _pump_until_counts(4, 2, 2):
		_fail("Live vector-style edit did not replace the raster generation")
		return false
	if not _validate_attachment_range(2, 4, Color(0.0, 0.0, 1.0, 1.0)):
		return false
	for binding in first_bindings:
		if binding.attached or binding.texture != null or binding.material != null:
			_fail("Style refresh retained stale raster renderer resources")
			return false
	for snapshot in receiver.detachments:
		if (
			not snapshot.attached_during_callback or
			not snapshot.texture_valid_during_callback or
			not snapshot.material_valid_during_callback
		):
			_fail("GeoJSON detachment callback ran after resources were cleared")
			return false
	var refreshed_ids := PackedInt64Array([
		int(receiver.attachments[2].texture_id),
		int(receiver.attachments[3].texture_id),
	])
	if first_texture_ids[0] in refreshed_ids or first_texture_ids[1] in refreshed_ids:
		_fail("Style refresh reused a stale raster texture generation")
		return false

	var attachment_start: int = receiver.attachments.size()
	var detachment_target: int = receiver.detachments.size() + 2
	var loaded_target: int = loaded_documents.size() + 1
	overlay.source = CesiumGeoJsonDocumentRasterOverlay.FromString
	overlay.geo_json = source_text
	if not await _pump_until_counts(
		attachment_start + 2, detachment_target, loaded_target
	):
		_fail("Inline GeoJSON did not parse asynchronously and rasterize")
		return false
	if not _validate_attachment_range(
		attachment_start,
		attachment_start + 2,
		Color(0.0, 0.0, 1.0, 1.0)
	):
		return false

	attachment_start = receiver.attachments.size()
	detachment_target = receiver.detachments.size() + 2
	loaded_target = loaded_documents.size() + 1
	overlay.source = CesiumGeoJsonDocumentRasterOverlay.FromUrl
	overlay.url = FIXTURE
	if not await _pump_until_counts(
		attachment_start + 2, detachment_target, loaded_target
	):
		_fail("res:// GeoJSON did not load asynchronously and rasterize")
		return false

	var remote_marker_count := _marker_count("geojson:/geojson.json:")
	attachment_start = receiver.attachments.size()
	detachment_target = receiver.detachments.size() + 2
	loaded_target = loaded_documents.size() + 1
	overlay.request_headers = {"X-Cesium-GeoJSON-Test": "header-preserved"}
	overlay.url = server_url + "/geojson.json"
	if not await _pump_until_counts(
		attachment_start + 2, detachment_target, loaded_target
	):
		_fail("HTTP GeoJSON did not load and rasterize")
		return false
	if (
		_marker_count("geojson:/geojson.json:") <= remote_marker_count or
		not _marker_contains("geojson:/geojson.json:header-preserved")
	):
		_fail("HTTP GeoJSON did not preserve custom request headers")
		return false

	# A newer generation must win if an older remote parse is still pending.
	var slow_marker_count := _marker_count("geojson:/slow-geojson.json:")
	loaded_target = loaded_documents.size() + 1
	overlay.url = server_url + "/slow-geojson.json"
	if not await _pump_until_marker_count(
		"geojson:/slow-geojson.json:", slow_marker_count + 1
	):
		_fail("Slow GeoJSON request did not begin")
		return false
	overlay.url = FIXTURE
	if not await _pump_until_loaded(loaded_target):
		_fail("Replacement local GeoJSON generation did not load")
		return false
	var replacement_loaded_count := loaded_documents.size()
	if not await _pump_for_milliseconds(1000):
		return false
	if loaded_documents.size() != replacement_loaded_count:
		_fail("Superseded slow GeoJSON generation emitted a stale loaded signal")
		return false

	failures.clear()
	overlay.source = CesiumGeoJsonDocumentRasterOverlay.FromString
	overlay.geo_json = "{ deliberately malformed"
	if not await _pump_until_failure(1):
		_fail("Malformed inline GeoJSON did not report parser diagnostics")
		return false
	if failures[0].errors.is_empty() or overlay.last_document_errors.is_empty():
		_fail("Malformed GeoJSON failure omitted parser errors")
		return false

	# Freeing a provider while a remote request is pending must be harmless.
	var doomed_marker_count := _marker_count("geojson:/slow-geojson.json:")
	var doomed := CesiumGeoJsonDocumentRasterOverlay.new()
	doomed.source = CesiumGeoJsonDocumentRasterOverlay.FromUrl
	doomed.url = server_url + "/slow-geojson.json"
	doomed.request_headers = {"X-Cesium-GeoJSON-Test": "freed-provider"}
	doomed.document_loaded.connect(_on_document_loaded)
	doomed.document_load_failed.connect(_on_document_load_failed)
	tileset.add_child(doomed)
	if not await _pump_until_marker_count(
		"geojson:/slow-geojson.json:", doomed_marker_count + 1
	):
		_fail("Disposable slow GeoJSON request did not begin")
		return false
	var loaded_before_removal := loaded_documents.size()
	var failures_before_removal := failures.size()
	doomed.remove_from_tileset(tileset)
	if doomed.is_added_to_tileset():
		_fail("Manually removed GeoJSON provider retained its Native instance")
		return false
	if not await _pump_for_milliseconds(1000):
		return false
	if (
		loaded_documents.size() != loaded_before_removal or
		failures.size() != failures_before_removal
	):
		_fail("Manually removed GeoJSON provider emitted a stale result signal")
		return false
	doomed.free()
	return true


func _set_style_color(style: CesiumVectorStyle, color: Color) -> void:
	style.line_color = color
	style.polygon_fill_color = color
	style.polygon_fill_enabled = true
	style.polygon_outline_enabled = false


func _validate_attachment_range(first: int, end: int, expected: Color) -> bool:
	var tile_ids: Dictionary = {}
	for index in range(first, end):
		var snapshot: Dictionary = receiver.attachments[index]
		if (
			snapshot.overlay_key != "Overlay0" or
			int(snapshot.image_width) <= 0 or int(snapshot.image_height) <= 0 or
			int(snapshot.coordinate_index) < 0 or
			not bool(snapshot.material_valid) or
			int(snapshot.texture_id) != int(snapshot.shader_texture_id) or
			not _color_near(snapshot.center_pixel, expected) or
			not _color_near(snapshot.quarter_pixel, expected)
		):
			_fail("GeoJSON raster attachment was incomplete or wrong-colored: %s" % [snapshot])
			return false
		tile_ids[snapshot.tile_id] = true
	if tile_ids.size() != end - first:
		_fail("GeoJSON fixture did not cover distinct streamed geometry tiles")
		return false
	return true


func _color_near(actual: Color, expected: Color) -> bool:
	return (
		abs(actual.r - expected.r) <= 0.02 and
		abs(actual.g - expected.g) <= 0.02 and
		abs(actual.b - expected.b) <= 0.02 and
		abs(actual.a - expected.a) <= 0.02
	)


func _pump_until_counts(
	required_attachments: int,
	required_detachments: int,
	required_loaded: int
) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if (
			receiver.attachments.size() >= required_attachments and
			receiver.detachments.size() >= required_detachments and
			loaded_documents.size() >= required_loaded
		):
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


func _pump_until_loaded(required_count: int) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if loaded_documents.size() >= required_count:
			return true
		await process_frame
	return false


func _pump_until_failure(required_count: int) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if failures.size() >= required_count:
			return true
		await process_frame
	return false


func _pump_until_marker_count(prefix: String, required_count: int) -> bool:
	for _frame in range(TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if _marker_count(prefix) >= required_count:
			return true
		await process_frame
	return false


func _pump_for_milliseconds(milliseconds: int) -> bool:
	var deadline := Time.get_ticks_msec() + milliseconds
	while Time.get_ticks_msec() < deadline:
		if tileset == null or not is_instance_valid(tileset):
			_fail("Tileset became invalid during asynchronous GeoJSON drain")
			return false
		tileset.update_tileset(camera.global_transform)
		await process_frame
	return true


func _marker_count(prefix: String) -> int:
	if not FileAccess.file_exists(request_marker):
		return 0
	var file := FileAccess.open(request_marker, FileAccess.READ)
	if file == null:
		return 0
	var count := 0
	for line in file.get_as_text().split("\n"):
		if line.begins_with(prefix):
			count += 1
	file.close()
	return count


func _marker_contains(value: String) -> bool:
	if not FileAccess.file_exists(request_marker):
		return false
	var file := FileAccess.open(request_marker, FileAccess.READ)
	if file == null:
		return false
	var contents := file.get_as_text()
	file.close()
	return contents.find(value) >= 0


func _on_document_loaded(document: CesiumGeoJsonDocument) -> void:
	loaded_documents.append(document)


func _on_document_load_failed(
	errors: PackedStringArray,
	warnings: PackedStringArray
) -> void:
	failures.append({"errors": errors, "warnings": warnings})


func _release_references() -> void:
	if receiver != null and is_instance_valid(receiver):
		receiver.attachments.clear()
		receiver.detachments.clear()
		receiver.events.clear()
	loaded_documents.clear()
	failures.clear()
	receiver = null
	overlay = null
	tileset = null


func _fail(message: String) -> void:
	push_error(message)
	_release_references()
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
