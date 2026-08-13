extends SceneTree

const LOAD_TIMEOUT_FRAMES := 600
const EARTH_RADIUS := 6_378_137.0

var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference
var tileset: Cesium3DTileset
var completion_count := 0
var completed_results: Array = []
var completed_warnings := PackedStringArray()
var cancellation_count := 0


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	test_root = Node3D.new()
	test_root.name = "HeightSamplingTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	camera.position = Vector3(EARTH_RADIUS + 400.0, 0.0, 0.0)
	test_root.add_child(camera)
	camera.look_at(Vector3(EARTH_RADIUS, 0.0, 0.0), Vector3.UP)
	georeference = CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)
	tileset = Cesium3DTileset.new()
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/height_sampling/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	georeference.add_child(tileset)

	var exact_input := PackedFloat64Array([
		0.000000123, 0.0, 456.0,
		1.0, 0.0, 321.0,
	])
	var request := tileset.sample_height_most_detailed_exact(exact_input)
	if request == null:
		_fail("Exact height query did not return a request")
		return
	if request.is_finished():
		_fail("Height query completed synchronously before signals could connect")
		return
	request.completed.connect(_on_completed)

	for _frame in range(LOAD_TIMEOUT_FRAMES):
		tileset.update_tileset(camera.global_transform)
		if request.is_finished():
			break
		await process_frame

	if not request.is_finished() or request.is_cancelled():
		_fail("Most-detailed height query did not complete")
		return
	if completion_count != 1:
		_fail("Height query completion signal count was %d" % completion_count)
		return
	if not _check_results(request):
		return
	if not _check_recreation_cancellation():
		return

	print("Cesium asynchronous height-sampling test passed")
	test_root.free()
	quit(0)


func _on_completed(results: Array, warnings: PackedStringArray) -> void:
	completion_count += 1
	completed_results = results
	completed_warnings = warnings


func _check_results(request: CesiumSampleHeightMostDetailedRequest) -> bool:
	if completed_warnings.size() != 0 or request.warnings.size() != 0:
		_fail("Successful height query returned warnings: %s" % [request.warnings])
		return false
	if completed_results.size() != 2 or request.results.size() != 2:
		_fail("Height query did not preserve input count/order")
		return false
	var hit := completed_results[0] as CesiumSampleHeightResult
	var miss := completed_results[1] as CesiumSampleHeightResult
	if hit == null or miss == null:
		_fail("Height results were not typed CesiumSampleHeightResult objects")
		return false
	var hit_exact: PackedFloat64Array = (
		hit.longitude_latitude_height_components
	)
	var miss_exact: PackedFloat64Array = (
		miss.longitude_latitude_height_components
	)
	if (
		not hit.sample_success or
		absf(hit_exact[0] - 0.000000123) > 0.000000000001 or
		absf(hit_exact[1]) > 0.000000000001 or
		absf(hit_exact[2] - 100.0) > 0.01
	):
		_fail("Expected the first exact sample at 100m, got %s" % [hit_exact])
		return false
	if miss.sample_success or absf(miss_exact[2] - 321.0) > 0.000001:
		_fail("Failed sample did not preserve its original height: %s" % [miss_exact])
		return false
	return true


func _check_recreation_cancellation() -> bool:
	var pending := tileset.sample_height_most_detailed(
		PackedVector3Array([Vector3(0.0, 0.0, 0.0)])
	)
	if pending == null or pending.is_finished():
		_fail("Convenience height query did not begin asynchronously")
		return false
	pending.cancelled.connect(func(_warnings: PackedStringArray) -> void:
		cancellation_count += 1
	)
	tileset.url = "file:///height-query-source-recreated"
	if (
		not pending.is_cancelled() or not pending.is_finished() or
		cancellation_count != 1 or pending.warnings.size() != 1
	):
		_fail(
			(
				"Source recreation cancellation mismatch: cancelled=%s " +
				"finished=%s signals=%d warnings=%s"
			) % [
				str(pending.is_cancelled()),
				str(pending.is_finished()),
				cancellation_count,
				str(pending.warnings),
			]
		)
		return false
	var retained := completed_results[0] as CesiumSampleHeightResult
	if (
		retained == null or not retained.sample_success or
		absf(retained.longitude_latitude_height_components[2] - 100.0) > 0.01
	):
		_fail("Completed height result did not survive source destruction")
		return false
	return true


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
