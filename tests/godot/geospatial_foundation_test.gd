extends SceneTree

const WGS84_X := 6378137.0
const WGS84_Z := 6356752.3142451793

var test_root: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	for runtime_class in [
		"CesiumEllipsoid",
		"CesiumGeoreference",
		"CesiumGlobeAnchor",
		"CesiumOriginShift",
		"GeoreferencedMesh",
	]:
		if not ClassDB.class_exists(runtime_class):
			_fail("Missing geospatial runtime class: " + runtime_class)
			return

	if not _test_ellipsoid():
		return
	if not _test_georeference_transforms():
		return
	if not _test_globe_anchor_and_origin_shift():
		return

	print("Geospatial foundation test passed")
	quit(0)


func _test_ellipsoid() -> bool:
	var ellipsoid: Resource = ClassDB.instantiate("CesiumEllipsoid")
	var radii := ellipsoid.call("get_radii_precise") as PackedFloat64Array
	if radii.size() != 3:
		return _fail_bool("WGS84 radii did not retain a float64 triple")
	if not _near(radii[0], WGS84_X, 1.0e-9) or not _near(radii[2], WGS84_Z, 1.0e-9):
		return _fail_bool("WGS84 radii are wrong: %s" % radii)

	var equator := ellipsoid.call(
		"longitude_latitude_height_to_ecef_precise",
		PackedFloat64Array([0.0, 0.0, 0.0])
	) as PackedFloat64Array
	if not _triple_near(equator, PackedFloat64Array([WGS84_X, 0.0, 0.0]), 1.0e-7):
		return _fail_bool("LLH-to-ECEF equator conversion is wrong: %s" % equator)
	var llh := ellipsoid.call(
		"ecef_to_longitude_latitude_height_precise",
		equator
	) as PackedFloat64Array
	if not _triple_near(llh, PackedFloat64Array([0.0, 0.0, 0.0]), 1.0e-9):
		return _fail_bool("ECEF-to-LLH round trip is wrong: %s" % llh)

	var polar_hit := ellipsoid.call(
		"ray_intersection_precise",
		PackedFloat64Array([0.0, 0.0, 7000000.0]),
		PackedFloat64Array([0.0, 0.0, -1.0])
	) as PackedFloat64Array
	if polar_hit.size() != 3 or not _near(polar_hit[2], WGS84_Z, 1.0e-6):
		return _fail_bool(
			"Ray intersection used a sphere instead of the ellipsoid: %s" % polar_hit
		)
	var miss := ellipsoid.call(
		"ray_intersection_precise",
		PackedFloat64Array([0.0, 0.0, 7000000.0]),
		PackedFloat64Array([0.0, 0.0, 1.0])
	) as PackedFloat64Array
	if not miss.is_empty():
		return _fail_bool("Outward ellipsoid ray unexpectedly hit: %s" % miss)
	return true


func _test_georeference_transforms() -> bool:
	test_root = Node3D.new()
	test_root.name = "GeospatialTransformTest"
	root.add_child(test_root)
	var georeference := ClassDB.instantiate("CesiumGeoreference") as Node3D
	test_root.add_child(georeference)
	georeference.set("origin_type", 0)
	georeference.call(
		"set_origin_longitude_latitude_height_precise",
		PackedFloat64Array([0.0, 0.0, 0.0])
	)

	var local_origin := georeference.call(
		"transform_ecef_position_to_local_precise",
		PackedFloat64Array([WGS84_X, 0.0, 0.0])
	) as PackedFloat64Array
	if not _triple_near(local_origin, PackedFloat64Array([0.0, 0.0, 0.0]), 1.0e-7):
		return _fail_bool("Cartographic origin is not local zero: %s" % local_origin)
	var east_ecef := georeference.call(
		"transform_local_position_to_ecef_precise",
		PackedFloat64Array([100.0, 0.0, 0.0])
	) as PackedFloat64Array
	if not _triple_near(east_ecef, PackedFloat64Array([WGS84_X, 100.0, 0.0]), 1.0e-7):
		return _fail_bool("Godot +X does not point east: %s" % east_ecef)
	var up_ecef := georeference.call(
		"transform_local_position_to_ecef_precise",
		PackedFloat64Array([0.0, 10.0, 0.0])
	) as PackedFloat64Array
	if not _triple_near(up_ecef, PackedFloat64Array([WGS84_X + 10.0, 0.0, 0.0]), 1.0e-7):
		return _fail_bool("Godot +Y does not point up: %s" % up_ecef)

	var precise_local := PackedFloat64Array([12345.678901234, -23.456789012, 98.765432109])
	var precise_ecef := georeference.call(
		"transform_local_position_to_ecef_precise",
		precise_local
	) as PackedFloat64Array
	var precise_round_trip := georeference.call(
		"transform_ecef_position_to_local_precise",
		precise_ecef
	) as PackedFloat64Array
	if not _triple_near(precise_round_trip, precise_local, 1.0e-8):
		return _fail_bool("Float64 local/ECEF round trip lost precision: %s" % precise_round_trip)

	georeference.set("origin_type", 1)
	var true_origin := georeference.call(
		"transform_local_position_to_ecef_precise",
		PackedFloat64Array([1.0, 2.0, 3.0])
	) as PackedFloat64Array
	if not _triple_near(true_origin, PackedFloat64Array([1.0, -3.0, 2.0]), 1.0e-12):
		return _fail_bool("True-origin Godot/native axis conversion is wrong: %s" % true_origin)
	georeference.set("scale_factor", 2.0)
	var scaled := georeference.call(
		"transform_local_position_to_ecef_precise",
		PackedFloat64Array([2.0, 4.0, 6.0])
	) as PackedFloat64Array
	if not _triple_near(scaled, PackedFloat64Array([1.0, -3.0, 2.0]), 1.0e-12):
		return _fail_bool("Georeference scale was not applied exactly: %s" % scaled)

	var custom_ellipsoid: Resource = ClassDB.instantiate("CesiumEllipsoid")
	custom_ellipsoid.call("set_radii_precise", PackedFloat64Array([10.0, 10.0, 5.0]))
	georeference.set("ellipsoid", custom_ellipsoid)
	georeference.set("origin_type", 0)
	georeference.call(
		"set_origin_longitude_latitude_height_precise",
		PackedFloat64Array([0.0, 0.0, 0.0])
	)
	if not _triple_near(
		georeference.call("get_origin_ecef_precise") as PackedFloat64Array,
		PackedFloat64Array([10.0, 0.0, 0.0]),
		1.0e-12
	):
		return _fail_bool("Custom ellipsoid was not used by the georeference")
	custom_ellipsoid.call("set_radii_precise", PackedFloat64Array([20.0, 20.0, 10.0]))
	if not _triple_near(
		georeference.call("get_origin_ecef_precise") as PackedFloat64Array,
		PackedFloat64Array([20.0, 0.0, 0.0]),
		1.0e-12
	):
		return _fail_bool("Live ellipsoid edits did not preserve the cartographic origin")

	test_root.free()
	test_root = null
	return true


func _test_globe_anchor_and_origin_shift() -> bool:
	test_root = Node3D.new()
	test_root.name = "GlobeAnchorTest"
	root.add_child(test_root)
	var georeference := ClassDB.instantiate("CesiumGeoreference") as Node3D
	test_root.add_child(georeference)
	georeference.set("origin_type", 0)
	georeference.call(
		"set_origin_longitude_latitude_height_precise",
		PackedFloat64Array([0.0, 0.0, 0.0])
	)

	var target := Node3D.new()
	target.name = "OrdinaryAnchoredNode"
	target.position = Vector3(100.0, 20.0, -50.0)
	georeference.add_child(target)
	var anchor := ClassDB.instantiate("CesiumGlobeAnchor") as Node
	target.add_child(anchor)
	if not anchor.call("is_anchor_initialized"):
		return _fail_bool("Component-style globe anchor did not initialize")
	var original_ecef := anchor.call("get_ecef_position_precise") as PackedFloat64Array
	if original_ecef.size() != 3:
		return _fail_bool("Globe anchor did not retain precise ECEF state")

	if not georeference.call("set_origin_ecef_precise", original_ecef):
		return _fail_bool("Georeference rejected the anchor ECEF position")
	if target.position.length() > 1.0e-3:
		return _fail_bool(
			"Anchored ordinary node did not follow the shifted origin: %s" % target.position
		)
	var retained_ecef := anchor.call("get_ecef_position_precise") as PackedFloat64Array
	if not _triple_near(retained_ecef, original_ecef, 1.0e-7):
		return _fail_bool("Origin shift changed the anchor's authoritative ECEF position")

	target.position = Vector3(25.0, 0.0, 0.0)
	if not anchor.call("sync_from_target"):
		return _fail_bool("Globe anchor could not capture an ordinary Node3D move")
	var moved_ecef := anchor.call("get_ecef_position_precise") as PackedFloat64Array
	if _distance(moved_ecef, retained_ecef) < 24.9:
		return _fail_bool("Globe anchor did not update after its target moved")

	var origin_shift := ClassDB.instantiate("CesiumOriginShift") as Node
	target.add_child(origin_shift)
	origin_shift.set("mode", 1)
	origin_shift.set("distance", 1.0)
	if not origin_shift.call("shift_origin_now"):
		return _fail_bool("Explicit floating-origin shift failed")
	if target.position.length() > 1.0e-3:
		return _fail_bool("Origin shift did not recenter its anchored target: %s" % target.position)
	if origin_shift.call("get_shift_count") != 1:
		return _fail_bool("Origin-shift diagnostics did not count the shift")

	target.free()
	georeference.call("refresh_dependents")
	test_root.free()
	test_root = null
	return true


func _near(left: float, right: float, tolerance: float) -> bool:
	return absf(left - right) <= tolerance


func _triple_near(
	left: PackedFloat64Array,
	right: PackedFloat64Array,
	tolerance: float
) -> bool:
	if left.size() != 3 or right.size() != 3:
		return false
	for index in range(3):
		if not _near(left[index], right[index], tolerance):
			return false
	return true


func _distance(left: PackedFloat64Array, right: PackedFloat64Array) -> float:
	if left.size() != 3 or right.size() != 3:
		return 0.0
	var x := left[0] - right[0]
	var y := left[1] - right[1]
	var z := left[2] - right[2]
	return sqrt(x * x + y * y + z * z)


func _fail_bool(message: String) -> bool:
	_fail(message)
	return false


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
