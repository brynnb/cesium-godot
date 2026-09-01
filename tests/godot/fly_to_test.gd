extends SceneTree

const WGS84_X := 6_378_137.0

var test_root: Node3D
var started_count := 0
var progressed_count := 0
var completed_count := 0
var interrupted_reasons: Array[int] = []
var started_destination := PackedFloat64Array()
var started_distance := 0.0


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if not ClassDB.class_exists("CesiumFlyTo"):
		_fail("CesiumFlyTo was not registered")
		return

	test_root = Node3D.new()
	test_root.name = "CesiumFlyToTest"
	root.add_child(test_root)
	var georeference := CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.CartographicOrigin
	georeference.set_origin_longitude_latitude_height_precise(
		PackedFloat64Array([0.0, 0.0, 0.0])
	)
	test_root.add_child(georeference)

	var target := Node3D.new()
	target.name = "FlightTarget"
	target.position = Vector3(0.0, 100.0, 0.0)
	target.scale = Vector3(2.0, 3.0, 4.0)
	georeference.add_child(target)
	var anchor := CesiumGlobeAnchor.new()
	anchor.automatic_transform_sync = false
	target.add_child(anchor)
	var fly := CesiumFlyTo.new()
	fly.automatic_update = false
	fly.duration = 1.0
	fly.progress_curve = null
	fly.maximum_height_by_distance_curve = null
	fly.default_maximum_height = 1000.0
	var height_curve := Curve.new()
	height_curve.min_value = 0.0
	height_curve.max_value = 1.0
	height_curve.add_point(Vector2(0.0, 0.0))
	height_curve.add_point(Vector2(0.5, 1.0))
	height_curve.add_point(Vector2(1.0, 0.0))
	fly.height_percentage_curve = height_curve
	target.add_child(fly)

	fly.flight_started.connect(_on_flight_started)
	fly.flight_progressed.connect(_on_flight_progressed)
	fly.flight_completed.connect(_on_flight_completed)
	fly.flight_interrupted.connect(_on_flight_interrupted)

	var destination := PackedFloat64Array([10.0, 0.0, 200.0])
	var destination_ecef := georeference.ellipsoid.longitude_latitude_height_to_ecef_precise(
		destination
	)
	if not fly.fly_to_location_longitude_latitude_height_precise(
		destination,
		90.0,
		20.0,
		true
	):
		_fail("Could not start a precise cartographic flight")
		return
	if (
		not fly.is_flight_in_progress() or
		started_count != 1 or
		started_distance < 1_000_000.0 or
		not _triple_near(started_destination, destination_ecef, 1.0e-6)
	):
		_fail("Flight-start state or diagnostics were incomplete")
		return
	if fly.fly_to_location_ecef_precise(destination_ecef):
		_fail("A second flight started while the first was active")
		return

	if not fly.update_flight(0.5):
		_fail("The first half of the flight did not advance")
		return
	var halfway_llh := (
		anchor.get_longitude_latitude_height_precise() as PackedFloat64Array
	)
	if (
		absf(fly.flight_progress - 0.5) > 1.0e-12 or
		absf(fly.current_height_offset - 1000.0) > 1.0e-6 or
		halfway_llh.size() != 3 or
		halfway_llh[0] < 4.9 or halfway_llh[0] > 5.1 or
		halfway_llh[2] < 1090.0
	):
		_fail("The ellipsoid arc or configurable height curve was not applied: %s" % [halfway_llh])
		return
	if not _matrix_scale_near(anchor.get_anchor_to_ecef_matrix_precise(), Vector3(2.0, 3.0, 4.0)):
		_fail("Flight interpolation did not preserve the target scale")
		return

	# Moving the local origin must not look like gameplay movement because the
	# anchor's authoritative ECEF state did not change.
	georeference.set_origin_longitude_latitude_height_precise(
		PackedFloat64Array([5.0, 0.0, 0.0])
	)
	if not fly.update_flight(0.25) or not fly.is_flight_in_progress():
		_fail("A floating-origin change interrupted the ECEF flight")
		return
	if not fly.update_flight(0.25):
		_fail("The final flight step did not run")
		return
	if (
		fly.is_flight_in_progress() or
		completed_count != 1 or
		progressed_count != 3 or
		absf(fly.flight_progress - 1.0) > 1.0e-12 or
		not _triple_near(anchor.get_ecef_position_precise(), destination_ecef, 1.0e-6)
	):
		_fail("Flight did not complete at the exact destination")
		return
	if not _matrix_scale_near(anchor.get_anchor_to_ecef_matrix_precise(), Vector3(2.0, 3.0, 4.0)):
		_fail("The completed flight changed the target scale")
		return

	# Gameplay movement of the anchor interrupts a cancellable flight and the
	# component must leave the external position untouched.
	fly.duration = 2.0
	if not fly.fly_to_location_longitude_latitude_height_precise(
		PackedFloat64Array([11.0, 0.0, 300.0])
	):
		_fail("Could not start the interruption flight")
		return
	if not fly.update_flight(0.25):
		_fail("The interruption flight did not advance")
		return
	var before_external_ecef := (
		anchor.get_ecef_position_precise() as PackedFloat64Array
	)
	var external_target_position := target.global_position + Vector3(25.0, 0.0, 0.0)
	target.global_position = external_target_position
	if fly.update_flight(0.25):
		_fail("A target-moved flight reported successful advancement")
		return
	var externally_moved := anchor.get_ecef_position_precise() as PackedFloat64Array
	if (
		fly.is_flight_in_progress() or
		interrupted_reasons != [CesiumFlyTo.TargetMoved] or
		fly.last_interruption_reason != CesiumFlyTo.TargetMoved or
		target.global_position.distance_to(external_target_position) > 1.0e-6 or
		_distance(externally_moved, before_external_ecef) < 24.9
	):
		_fail("External movement did not interrupt cleanly: %s" % [interrupted_reasons])
		return

	# Explicit cancellation is one-shot and does not invent a completion.
	if not fly.fly_to_location_local_precise(
		PackedFloat64Array([1000.0, 500.0, -2000.0]),
		0.0,
		0.0,
		false
	):
		_fail("Could not start a precise local-coordinate flight")
		return
	if not fly.interrupt_flight() or fly.interrupt_flight():
		_fail("Explicit interruption was not one-shot")
		return
	if (
		interrupted_reasons != [
			CesiumFlyTo.TargetMoved,
			CesiumFlyTo.ExplicitlyInterrupted,
		] or
		completed_count != 1
	):
		_fail("Explicit interruption emitted the wrong terminal signals")
		return

	# A zero-duration flight still commits its exact endpoint and emits one
	# progress update followed by completion when manually advanced.
	fly.duration = 0.0
	var zero_destination := PackedFloat64Array([9.5, 0.25, 50.0])
	if not fly.fly_to_location_longitude_latitude_height_precise(
		zero_destination,
		-45.0,
		-15.0,
		false
	) or not fly.update_flight(0.0):
		_fail("Zero-duration flight did not complete deterministically")
		return
	var zero_destination_ecef := georeference.ellipsoid.longitude_latitude_height_to_ecef_precise(
		zero_destination
	)
	if (
		completed_count != 2 or
		fly.is_flight_in_progress() or
		not _triple_near(anchor.get_ecef_position_precise(), zero_destination_ecef, 1.0e-6)
	):
		_fail("Zero-duration flight missed its exact endpoint")
		return

	# The normal process callback advances automatically, while losing the
	# anchor during an active manual flight terminates exactly once.
	fly.automatic_update = true
	if not fly.fly_to_location_longitude_latitude_height_precise(
		PackedFloat64Array([9.25, 0.0, 75.0]),
		0.0,
		0.0,
		false
	):
		_fail("Could not start an automatically updated flight")
		return
	# `process_frame` is emitted before Node._process. Wait once to enter the
	# next frame and once more for the automatic process callback to run.
	await process_frame
	await process_frame
	if completed_count != 3 or fly.is_flight_in_progress():
		_fail("Automatic process advancement did not complete the flight")
		return
	fly.automatic_update = false
	fly.duration = 1.0
	if not fly.fly_to_location_longitude_latitude_height_precise(
		PackedFloat64Array([9.0, 0.0, 100.0]),
		0.0,
		0.0,
		false
	):
		_fail("Could not start the anchor-lifetime flight")
		return
	anchor.free()
	if fly.update_flight(0.1):
		_fail("A flight advanced after its globe anchor was destroyed")
		return
	if (
		interrupted_reasons != [
			CesiumFlyTo.TargetMoved,
			CesiumFlyTo.ExplicitlyInterrupted,
			CesiumFlyTo.AnchorUnavailable,
		] or
		fly.is_flight_in_progress()
	):
		_fail("Anchor destruction did not emit one typed interruption")
		return

	print("Cesium ellipsoid-aware fly-to test passed")
	test_root.free()
	test_root = null
	quit(0)


func _on_flight_started(destination_ecef: PackedFloat64Array, distance: float) -> void:
	started_count += 1
	started_destination = destination_ecef.duplicate()
	started_distance = distance


func _on_flight_progressed(
	_progress: float,
	_position_ecef: PackedFloat64Array,
	_height_offset: float
) -> void:
	progressed_count += 1


func _on_flight_completed() -> void:
	completed_count += 1


func _on_flight_interrupted(reason: int) -> void:
	interrupted_reasons.append(reason)


func _matrix_scale_near(
	matrix: PackedFloat64Array,
	expected: Vector3
) -> bool:
	if matrix.size() != 16:
		return false
	for column in range(3):
		var start := column * 4
		var magnitude := sqrt(
			matrix[start] * matrix[start] +
			matrix[start + 1] * matrix[start + 1] +
			matrix[start + 2] * matrix[start + 2]
		)
		if absf(magnitude - expected[column]) > 1.0e-7:
			return false
	return true


func _triple_near(
	left: PackedFloat64Array,
	right: PackedFloat64Array,
	tolerance: float
) -> bool:
	if left.size() != 3 or right.size() != 3:
		return false
	for index in range(3):
		if absf(left[index] - right[index]) > tolerance:
			return false
	return true


func _distance(left: PackedFloat64Array, right: PackedFloat64Array) -> float:
	if left.size() != 3 or right.size() != 3:
		return 0.0
	var x := left[0] - right[0]
	var y := left[1] - right[1]
	var z := left[2] - right[2]
	return sqrt(x * x + y * y + z * z)


func _fail(message: String) -> void:
	push_error(message)
	if test_root != null and is_instance_valid(test_root):
		test_root.free()
	quit(1)
