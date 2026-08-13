extends SceneTree

const TIMEOUT_MSEC := 5000
const EPSILON := 0.000000001

var server_url := ""
var request_marker := ""
var service: CesiumGeocoderService
var completed_signal_count := 0
var failed_signal_count := 0
var cancelled_signal_count := 0
var endpoint_callback_request: CesiumGeocoderRequest


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	server_url = OS.get_environment("CESIUM_TEST_SERVER_URL")
	request_marker = OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	if server_url.is_empty() or request_marker.is_empty():
		_fail("Geocoder test server environment was not provided")
		return

	service = CesiumGeocoderService.new()
	service.name = "GeocoderServiceTest"
	service.api_url = server_url
	service.access_token = "deterministic-geocoder-token"
	service.worker_thread_count = 0
	service.maximum_network_retries = 99
	service.network_retry_initial_delay_seconds = 100.0
	service.network_retry_maximum_delay_seconds = 1.0
	root.add_child(service)

	if not _check_configuration_and_validation():
		return
	if not await _check_search_autocomplete_and_app_data_cache():
		return
	if not await _check_failure_and_cancellation():
		return

	print("Cesium geocoder service test passed")
	if service != null:
		service.free()
	service = null
	await process_frame
	quit(0)


func _check_configuration_and_validation() -> bool:
	var configuration: Dictionary = service.get_configuration()
	if (
		service.api_url != server_url + "/" or
		service.worker_thread_count != 1 or
		service.maximum_network_retries != 10 or
		abs(service.network_retry_initial_delay_seconds - 60.0) > EPSILON or
		abs(service.network_retry_maximum_delay_seconds - 60.0) > EPSILON or
		configuration.access_token_configured != true or
		str(configuration).find("deterministic-geocoder-token") >= 0
	):
		_fail("Geocoder configuration was not normalized or token-safe: %s" % [configuration])
		return false

	var invalid: CesiumGeocoderRequest = service.search("   ")
	if (
		invalid == null or
		invalid.status != CesiumGeocoderRequest.Failed or
		invalid.error_code != "InvalidQuery" or
		invalid.http_status_code != 0
	):
		_fail("Empty geocoder query did not fail synchronously and clearly")
		return false
	return true


func _check_search_autocomplete_and_app_data_cache() -> bool:
	# Submit both before a frame is pumped. They must share one appData request.
	var search_request: CesiumGeocoderRequest = service.search(
		"Kōjan & Harbor",
		CesiumGeocoderService.Bing
	)
	var autocomplete_request: CesiumGeocoderRequest = service.autocomplete(
		"Leth",
		CesiumGeocoderService.Google
	)
	search_request.completed.connect(_on_request_completed)
	autocomplete_request.completed.connect(_on_request_completed)
	var cancelled_while_queued: CesiumGeocoderRequest = service.search(
		"slow-cancel",
		CesiumGeocoderService.Default
	)
	cancelled_while_queued.cancelled.connect(_on_request_cancelled)
	cancelled_while_queued.cancel()
	if (
		cancelled_while_queued.status != CesiumGeocoderRequest.Cancelled or
		service.pending_request_count != 2
	):
		_fail("Queued geocoder cancellation did not update request ownership")
		return false

	if not await _pump_request(search_request):
		_fail("Geocoder search did not finish: %s" % [search_request.error_message])
		return false
	if not await _pump_request(autocomplete_request):
		_fail("Geocoder autocomplete did not finish: %s" % [autocomplete_request.error_message])
		return false
	if (
		search_request.status != CesiumGeocoderRequest.Completed or
		autocomplete_request.status != CesiumGeocoderRequest.Completed or
		search_request.provider != CesiumGeocoderService.Bing or
		search_request.request_type != CesiumGeocoderService.Search or
		autocomplete_request.provider != CesiumGeocoderService.Google or
		autocomplete_request.request_type != CesiumGeocoderService.Autocomplete or
		search_request.http_status_code != 200 or completed_signal_count != 2
	):
		_fail("Search/autocomplete request metadata or completion was incorrect")
		return false

	if not _check_result(search_request.result):
		return false
	var configuration: Dictionary = service.get_configuration()
	if (
		configuration.application_data_cached != true or
		configuration.pending_request_count != 0 or
		_marker_count("geocoder-app-data") != 1 or
		not _marker_contains("geocoder-request:search:bing:Kōjan & Harbor:auth=true") or
		not _marker_contains("geocoder-request:autocomplete:google:Leth:auth=true")
	):
		_fail("Geocoder app-data caching, query encoding, provider, or auth flow was incomplete")
		return false
	return true


func _check_result(result: CesiumGeocoderResult) -> bool:
	if result == null:
		_fail("Completed geocoder request had no result")
		return false
	var features: Array = result.features
	var attributions: Array = result.attributions
	if features.size() != 2 or attributions.size() != 2:
		_fail("Geocoder result did not preserve all features and attributions")
		return false
	var point: CesiumGeocoderFeature = features[0]
	var region: CesiumGeocoderFeature = features[1]
	var visible_credit: CesiumGeocoderAttribution = attributions[0]
	var popover_credit: CesiumGeocoderAttribution = attributions[1]
	var point_components: PackedFloat64Array = point.longitude_latitude_height_components
	var point_rectangle: PackedFloat64Array = point.globe_rectangle_components
	var region_components: PackedFloat64Array = region.longitude_latitude_height_components
	var region_rectangle: PackedFloat64Array = region.globe_rectangle_components
	if (
		point.display_name != "Kojan Harbor" or not point.is_point or
		point_components.size() != 3 or point_rectangle.size() != 4 or
		abs(point_components[0] - 145.125) > EPSILON or
		abs(point_components[1] + 37.875) > EPSILON or
		abs(point_rectangle[0] - 145.125) > EPSILON or
		region.display_name != "Leth Nurae Region" or region.is_point or
		abs(region_components[0] - 12.0) > EPSILON or
		abs(region_components[1] - 22.0) > EPSILON or
		abs(region_rectangle[0] - 10.0) > EPSILON or
		abs(region_rectangle[1] - 20.0) > EPSILON or
		abs(region_rectangle[2] - 14.0) > EPSILON or
		abs(region_rectangle[3] - 24.0) > EPSILON or
		not visible_credit.show_on_screen or popover_credit.show_on_screen or
		visible_credit.html.find("Visible geocoder credit") < 0
	):
		_fail("Typed geocoder result lost point, region, precision, or attribution data")
		return false
	# Returned arrays are defensive copies.
	features.clear()
	attributions.clear()
	if result.features.size() != 2 or result.attributions.size() != 2:
		_fail("Geocoder result arrays were mutable aliases")
		return false
	return true


func _check_failure_and_cancellation() -> bool:
	var failed: CesiumGeocoderRequest = service.search("force-failure")
	failed.failed.connect(_on_request_failed)
	if not await _pump_request(failed):
		_fail("Failed geocoder request did not resolve")
		return false
	if (
		failed.status != CesiumGeocoderRequest.Failed or
		failed.error_code != "401" or failed.http_status_code != 401 or
		failed_signal_count != 1 or
		failed.result != null
	):
		_fail("HTTP geocoder failure did not preserve its status and error")
		return false

	var cancelled: CesiumGeocoderRequest = service.search("slow-cancel")
	cancelled.cancelled.connect(_on_request_cancelled)
	await process_frame
	service.worker_thread_count = 3
	if service.get_configuration().runtime_recreation_pending != true:
		_fail("Active geocoder reconfiguration was not deferred safely")
		return false
	var after_reconfiguration: CesiumGeocoderRequest = service.autocomplete(
		"After reconfiguration"
	)
	after_reconfiguration.completed.connect(_on_request_completed)
	var draining: Dictionary = service.get_configuration()
	if (
		draining.pending_request_count != 2 or
		draining.deferred_request_count != 1
	):
		_fail("A request submitted during runtime recreation was not deferred")
		return false
	cancelled.cancel()
	if (
		cancelled.status != CesiumGeocoderRequest.Cancelled or
		not cancelled.is_finished() or not cancelled.is_cancelled() or
		service.pending_request_count != 1 or cancelled_signal_count != 2
	):
		_fail("In-flight logical cancellation did not complete immediately")
		return false
	if not await _pump_request(after_reconfiguration):
		_fail("A request deferred across runtime recreation did not finish")
		return false
	if cancelled.status != CesiumGeocoderRequest.Cancelled:
		_fail("Late Native geocoder completion overwrote cancellation")
		return false
	var reconfigured: Dictionary = service.get_configuration()
	if (
		reconfigured.runtime_recreation_pending != false or
		reconfigured.deferred_request_count != 0 or
		reconfigured.application_data_cached != true or
		service.worker_thread_count != 3 or
		after_reconfiguration.status != CesiumGeocoderRequest.Completed or
		_marker_count("geocoder-app-data") != 2
	):
		_fail("Deferred geocoder runtime reconfiguration did not finish at idle")
		return false

	# Change endpoints from the cancelled signal itself. The callback submits a
	# replacement request, which must wait for the old callback/runtime to drain.
	var endpoint_cancelled: CesiumGeocoderRequest = service.search("slow-cancel")
	endpoint_cancelled.cancelled.connect(_on_endpoint_request_cancelled)
	await process_frame
	service.api_url = server_url + "/alternate"
	if (
		endpoint_cancelled.status != CesiumGeocoderRequest.Cancelled or
		endpoint_callback_request == null or
		service.get_configuration().deferred_request_count != 1
	):
		_fail("Endpoint replacement did not cancel and defer callback work safely")
		return false
	if not await _pump_request(endpoint_callback_request):
		_fail("Request queued by endpoint-cancellation callback did not finish")
		return false
	if (
		endpoint_callback_request.status != CesiumGeocoderRequest.Completed or
		service.api_url != server_url + "/alternate/" or
		_marker_count("geocoder-app-data") != 3
	):
		_fail("Endpoint replacement request did not use the rebuilt runtime")
		return false

	var shutdown_request: CesiumGeocoderRequest = service.search("slow-cancel")
	shutdown_request.cancelled.connect(_on_request_cancelled)
	service.free()
	service = null
	await process_frame
	if (
		shutdown_request.status != CesiumGeocoderRequest.Cancelled or
		cancelled_signal_count != 4
	):
		_fail("Service shutdown did not cancel its owned request")
		return false
	return true


func _pump_request(request: CesiumGeocoderRequest) -> bool:
	var deadline := Time.get_ticks_msec() + TIMEOUT_MSEC
	while not request.is_finished() and Time.get_ticks_msec() < deadline:
		await process_frame
	return request.is_finished()


func _pump_for_milliseconds(milliseconds: int) -> void:
	var deadline := Time.get_ticks_msec() + milliseconds
	while Time.get_ticks_msec() < deadline:
		await process_frame


func _marker_contains(expected: String) -> bool:
	var file := FileAccess.open(request_marker, FileAccess.READ)
	return file != null and file.get_as_text().find(expected) >= 0


func _marker_count(prefix: String) -> int:
	var file := FileAccess.open(request_marker, FileAccess.READ)
	if file == null:
		return 0
	var count := 0
	for line in file.get_as_text().split("\n", false):
		if line.begins_with(prefix):
			count += 1
	return count


func _on_request_completed(_result: CesiumGeocoderResult) -> void:
	completed_signal_count += 1


func _on_request_failed(_code: String, _message: String, _status: int) -> void:
	failed_signal_count += 1


func _on_request_cancelled() -> void:
	cancelled_signal_count += 1


func _on_endpoint_request_cancelled() -> void:
	cancelled_signal_count += 1
	endpoint_callback_request = service.search("After endpoint change")


func _fail(message: String) -> void:
	push_error(message)
	if service != null and is_instance_valid(service):
		service.free()
	service = null
	quit(1)
