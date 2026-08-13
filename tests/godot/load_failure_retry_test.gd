extends SceneTree

const EARTH_RADIUS := 6_378_137.0
const TIMEOUT_MILLISECONDS := 5000

var test_root: Node3D
var georeference: CesiumGeoreference
var camera: Camera3D
var tileset: Cesium3DTileset
var failures: Array[CesiumLoadFailure] = []
var overlay_failures: Array[CesiumLoadFailure] = []
var server_url := ""
var marker_path := ""


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	server_url = OS.get_environment("CESIUM_TEST_SERVER_URL")
	marker_path = OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	if server_url.is_empty() or marker_path.is_empty():
		_fail("The tileset fixture server environment was not provided")
		return

	test_root = Node3D.new()
	test_root.name = "LoadFailureRetryTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)
	georeference = CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	if not await _test_request_headers():
		return
	if not await _test_transient_retry_then_success():
		return
	if not await _test_transport_exception_retry_then_success():
		return
	if not await _test_permanent_http_failure():
		return
	if not await _test_malformed_root_is_not_retried():
		return
	if not await _test_terminal_tile_content_failure():
		return
	if not await _test_overlay_failure_signal():
		return

	print("Cesium structured load-failure and retry test passed")
	await _release_tileset()
	test_root.free()
	quit(0)


func _test_request_headers() -> bool:
	const INITIAL_SECRET := "request-header-secret"
	const ROTATED_SECRET := "request-header-secret-rotated"
	var initial_root_baseline := _marker_count("tileset-header:root:initial")
	var initial_content_baseline := _marker_count("tileset-header:content:initial")
	var rotated_root_baseline := _marker_count("tileset-header:root:rotated")
	var rotated_content_baseline := _marker_count("tileset-header:content:rotated")
	_new_tileset(server_url + "/header-tileset.json")

	# Whitespace/case normalize, invalid entries disappear, and the source
	# Dictionary must not remain aliased to the node property.
	var supplied := {
		" X-Cesium-Tileset-Test ": "  " + INITIAL_SECRET + "  ",
		"": "ignored-empty-name",
		"empty-value": "",
		"line\nbreak": "ignored",
		"bad header name": "ignored-space-in-name",
		"bad@header": "ignored-invalid-token-character",
		42: "ignored-non-string-key",
		"non-string-value": 42,
	}
	tileset.request_headers = supplied
	supplied[" X-Cesium-Tileset-Test "] = "mutated-source"
	var returned_headers: Dictionary = tileset.request_headers
	returned_headers["x-cesium-tileset-test"] = "mutated-return-value"
	var configured_headers: Dictionary = tileset.request_headers
	if configured_headers != {
		"x-cesium-tileset-test": INITIAL_SECRET,
	}:
		_fail("Tileset request headers were not normalized and defensively copied: %s" % [
			configured_headers
		])
		return false

	if not await _pump_until(func() -> bool:
		return (
			_marker_count("tileset-header:root:initial") > initial_root_baseline and
			_marker_count("tileset-header:content:initial") > initial_content_baseline and
			tileset.get_child_count() > 0
		)
	):
		_fail("Root and content requests did not both receive tileset headers")
		return false

	var initial_root_count := _marker_count("tileset-header:root:initial")
	var initial_content_count := _marker_count("tileset-header:content:initial")
	# This is semantically identical after normalization. It must preserve the
	# active Native tileset rather than causing another root/content load.
	tileset.request_headers = {
		&"X-CESIUM-TILESET-TEST": INITIAL_SECRET,
		" ": "ignored",
		"ignored-empty": "   ",
	}
	var no_op_deadline := Time.get_ticks_msec() + 300
	while Time.get_ticks_msec() < no_op_deadline:
		tileset.update_tileset(camera.global_transform)
		await process_frame
	if (
		_marker_count("tileset-header:root:initial") != initial_root_count or
		_marker_count("tileset-header:content:initial") != initial_content_count
	):
		_fail("A semantically unchanged request-header map recreated the tileset")
		return false

	# A real value change must recreate the Native tileset, and the replacement
	# root/content requests must both use the new snapshot.
	tileset.request_headers = {
		"X-Cesium-Tileset-Test": ROTATED_SECRET,
	}
	if not await _pump_until(func() -> bool:
		return (
			_marker_count("tileset-header:root:rotated") > rotated_root_baseline and
			_marker_count("tileset-header:content:rotated") > rotated_content_baseline
		)
	):
		_fail("Changing a request header did not recreate with the new snapshot")
		return false

	# Configuration remains readable by its owner, but diagnostic surfaces must
	# never copy credential-bearing values.
	var diagnostic_text := str(tileset.get_streaming_statistics())
	for failure in failures:
		diagnostic_text += str(failure.to_dictionary())
	if (
		diagnostic_text.find(INITIAL_SECRET) >= 0 or
		diagnostic_text.find(ROTATED_SECRET) >= 0
	):
		_fail("A tileset request-header value leaked into diagnostics")
		return false

	await _release_tileset()
	return true


func _test_transient_retry_then_success() -> bool:
	_new_tileset(
		server_url + "/retry-tileset.json?access_token=diagnostic-secret"
	)
	if not await _pump_until(func() -> bool:
		return tileset.get_child_count() > 0 and _scheduled_network_failures().size() == 2
	):
		_fail("Transient 503 fixture did not retry twice and load successfully")
		return false

	var retries := _scheduled_network_failures()
	for index in range(2):
		var failure := retries[index]
		if (
			failure.category_name != "network" or
			failure.stage_name != "network_request" or
			failure.http_status_code != 503 or
			failure.terminal or
			not failure.retryable or
			not failure.retry_scheduled or
			failure.attempt != index + 1 or
			failure.maximum_attempts != 4 or
			failure.source != tileset or
			failure.url.find("diagnostic-secret") >= 0 or
			failure.url.find("<redacted>") < 0
		):
			_fail("Retry failure details were incomplete or exposed credentials: %s" % [
				failure.to_dictionary()
			])
			return false

	var statistics := tileset.get_streaming_statistics()
	if (
		int(statistics.get("request_retries", -1)) != 2 or
		int(statistics.get("request_attempts", 0)) < 4 or
		int(statistics.get("request_failures", -1)) != 0 or
		int(statistics.get("request_successes", 0)) < 2 or
		int(statistics.get("requests_in_flight", -1)) != 0 or
		int(statistics.get("request_retries_queued", -1)) != 0 or
		int(statistics.get("request_total_us", 0)) <= 0 or
		int(statistics.get("request_response_bytes", 0)) <= 0
	):
		_fail("Retry/request statistics were inconsistent: %s" % [statistics])
		return false

	await _release_tileset()
	return true


func _test_transport_exception_retry_then_success() -> bool:
	_new_tileset(server_url + "/disconnect-retry-tileset.json?api_key=hidden")
	if not await _pump_until(func() -> bool:
		return tileset.get_child_count() > 0 and _scheduled_network_failures().size() == 2
	):
		_fail("Disconnected transport fixture did not retry twice and recover")
		return false

	var retries := _scheduled_network_failures()
	for index in range(2):
		var failure := retries[index]
		if (
			failure.http_status_code != 0 or
			failure.attempt != index + 1 or
			failure.url.find("hidden") >= 0 or
			failure.url.find("<redacted>") < 0
		):
			_fail("Transport exception retry was misclassified: %s" % [
				failure.to_dictionary()
			])
			return false

	var statistics := tileset.get_streaming_statistics()
	if (
		int(statistics.get("request_retries", -1)) != 2 or
		int(statistics.get("request_failures", -1)) != 0 or
		int(statistics.get("request_successes", 0)) < 2 or
		int(statistics.get("requests_in_flight", -1)) != 0
	):
		_fail("Transport exception recovery statistics were inconsistent: %s" % [
			statistics
		])
		return false

	await _release_tileset()
	return true


func _test_permanent_http_failure() -> bool:
	_new_tileset(server_url + "/permanent-tileset.json")
	if not await _pump_until(func() -> bool:
		return _has_category("network") and _has_category("tileset")
	):
		_fail("Permanent 404 did not emit network and tileset failures")
		return false
	var network := _first_category("network")
	var root_failure := _first_category("tileset")
	if (
		network == null or network.http_status_code != 404 or
		network.retryable or network.retry_scheduled or not network.terminal or
		root_failure == null or root_failure.stage_name != "tileset_json" or
		root_failure.http_status_code != 404
	):
		_fail("Permanent failure was misclassified: %s" % [_failure_snapshots()])
		return false
	var statistics := tileset.get_streaming_statistics()
	if (
		int(statistics.get("request_attempts", -1)) != 1 or
		int(statistics.get("request_retries", -1)) != 0 or
		int(statistics.get("request_failures", -1)) != 1
	):
		_fail("Permanent 404 was retried or miscounted: %s" % [statistics])
		return false
	await _release_tileset()
	return true


func _test_malformed_root_is_not_retried() -> bool:
	_new_tileset(server_url + "/malformed-tileset.json")
	if not await _pump_until(func() -> bool: return _has_category("tileset")):
		_fail("Malformed 200 tileset did not emit a structured tileset failure")
		return false
	if _has_category("network"):
		_fail("Malformed successful HTTP response was classified as a network failure")
		return false
	var statistics := tileset.get_streaming_statistics()
	if (
		int(statistics.get("request_attempts", -1)) != 1 or
		int(statistics.get("request_retries", -1)) != 0 or
		int(statistics.get("request_successes", -1)) != 1
	):
		_fail("Malformed data was retried instead of failing permanently: %s" % [statistics])
		return false
	await _release_tileset()
	return true


func _test_terminal_tile_content_failure() -> bool:
	_new_tileset(server_url + "/malformed-content-tileset.json")
	if not await _pump_until(func() -> bool: return _has_category("tile_content")):
		_fail("Malformed leaf content did not emit an individual tile failure")
		return false
	var tile_failure := _first_category("tile_content")
	if (
		tile_failure == null or not tile_failure.terminal or
		tile_failure.stage_name != "tile_content_request" or
		tile_failure.tile_id.is_empty() or tile_failure.retry_scheduled
	):
		_fail("Tile failure lacked stable identity or terminal state: %s" % [
			_failure_snapshots()
		])
		return false
	var statistics := tileset.get_streaming_statistics()
	if int(statistics.get("failed", 0)) != 1:
		_fail("Terminal tile failure was not reflected in statistics: %s" % [statistics])
		return false
	await _release_tileset()
	return true


func _test_overlay_failure_signal() -> bool:
	_new_tileset(server_url + "/retry-tileset.json")
	var overlay := CesiumIonRasterOverlay.new()
	overlay.name = "BrokenOverlay"
	overlay.data_source = 1
	overlay.url = server_url + "/overlay/"
	overlay.key = "broken_overlay"
	overlay.load_failure.connect(_on_overlay_load_failure)
	tileset.add_child(overlay)
	if not await _pump_until(func() -> bool: return not overlay_failures.is_empty()):
		_fail("Raster overlay provider failure did not reach the overlay signal")
		return false
	var failure := overlay_failures[0]
	if (
		failure.source != overlay or
		failure.category_name != "raster_overlay" or
		failure.stage_name != "raster_tile_provider" or
		failure.overlay_key != "broken_overlay" or
		failure.http_status_code != 404
	):
		_fail("Overlay failure details were incomplete: %s" % [failure.to_dictionary()])
		return false
	if not failures.has(failure):
		_fail("Tileset did not receive the unified overlay failure event")
		return false
	await _release_tileset()
	return true


func _new_tileset(url: String) -> void:
	failures.clear()
	overlay_failures.clear()
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = url
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_network_retries = 3
	tileset.network_retry_initial_delay_seconds = 0.01
	tileset.network_retry_maximum_delay_seconds = 0.02
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.load_failure.connect(_on_load_failure)
	georeference.add_child(tileset)


func _release_tileset() -> void:
	if tileset != null:
		# These tests validate terminal classification, not shutdown cancellation.
		# Let any sibling content request finish so a test case cannot leak an
		# intentionally canceled Future into the next case or process shutdown.
		var drain_deadline := Time.get_ticks_msec() + 1000
		while Time.get_ticks_msec() < drain_deadline:
			var statistics := tileset.get_streaming_statistics()
			if int(statistics.get("requests_in_flight", 0)) == 0:
				break
			tileset.update_tileset(camera.global_transform)
			await process_frame
		tileset.free()
		tileset = null
	await process_frame
	failures.clear()
	overlay_failures.clear()


func _pump_until(condition: Callable) -> bool:
	var deadline := Time.get_ticks_msec() + TIMEOUT_MILLISECONDS
	while Time.get_ticks_msec() < deadline:
		if tileset != null:
			tileset.update_tileset(camera.global_transform)
		if condition.call():
			# Allow already-scheduled deferred diagnostic events to settle.
			await process_frame
			return true
		await process_frame
	return false


func _scheduled_network_failures() -> Array[CesiumLoadFailure]:
	var result: Array[CesiumLoadFailure] = []
	for failure in failures:
		if failure.category_name == "network" and failure.retry_scheduled:
			result.append(failure)
	return result


func _marker_count(value: String) -> int:
	var count := 0
	for line in FileAccess.get_file_as_string(marker_path).split("\n"):
		if line == value:
			count += 1
	return count


func _has_category(category_name: String) -> bool:
	return _first_category(category_name) != null


func _first_category(category_name: String) -> CesiumLoadFailure:
	for failure in failures:
		if failure.category_name == category_name:
			return failure
	return null


func _failure_snapshots() -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for failure in failures:
		result.append(failure.to_dictionary())
	return result


func _on_load_failure(failure: CesiumLoadFailure) -> void:
	failures.append(failure)


func _on_overlay_load_failure(failure: CesiumLoadFailure) -> void:
	overlay_failures.append(failure)


func _fail(message: String) -> void:
	push_error(message)
	if tileset != null:
		tileset.free()
	test_root.free()
	quit(1)
