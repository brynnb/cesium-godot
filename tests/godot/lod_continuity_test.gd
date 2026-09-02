extends SceneTree

const EARTH_RADIUS := 6_378_137.0
# Software Vulkan can render hundreds of frames per second after spending over
# a second compiling its first shader. Keep a generous frame cap so extended
# test-only transition durations remain bounded by time rather than frame rate.
const FRAME_TIMEOUT := 3000


class TrackingReceiver extends Cesium3DTilesetLifecycleEventReceiver:
	var loaded_ids: Array[String] = []
	var visible_by_id: Dictionary = {}
	var tiles_by_id: Dictionary = {}
	var events: Array[String] = []
	var use_unsupported_material := false

	func _init() -> void:
		material_selector = Callable(self, "_create_material")
		tile_loaded.connect(_on_tile_loaded)
		tile_visibility_changed.connect(_on_tile_visibility_changed)

	func _create_material(
		_primitive: CesiumLoadedTilePrimitive,
		default_material: Material
	) -> Material:
		if not use_unsupported_material:
			return default_material
		var shader := Shader.new()
		shader.code = """
shader_type spatial;
void fragment() {
	ALBEDO = vec3(0.2, 0.7, 0.4);
}
"""
		var material := ShaderMaterial.new()
		material.shader = shader
		return material

	func _on_tile_loaded(tile: Cesium3DTile) -> void:
		var tile_id := tile.tile_id
		loaded_ids.append(tile_id)
		tiles_by_id[tile_id] = tile
		events.append("loaded:%s" % tile_id)

	func _on_tile_visibility_changed(
		tile: Cesium3DTile,
		visible: bool
	) -> void:
		var tile_id := tile.tile_id
		visible_by_id[tile_id] = visible
		events.append("visible:%s:%s" % [tile_id, visible])


var test_root: Node3D
var camera: Camera3D
var georeference: CesiumGeoreference
var server_url := ""
var marker_path := ""
var transition_length := 0.5


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	server_url = OS.get_environment("CESIUM_TEST_SERVER_URL")
	marker_path = OS.get_environment("CESIUM_TEST_REQUEST_MARKER")
	var configured_transition_length := OS.get_environment(
		"CESIUM_TEST_LOD_TRANSITION_LENGTH"
	)
	if not configured_transition_length.is_empty():
		transition_length = maxf(0.001, configured_transition_length.to_float())
	if server_url.is_empty() or marker_path.is_empty():
		_fail("LOD fixture server environment was not configured")
		return

	test_root = Node3D.new()
	test_root.name = "LodContinuityTest"
	root.add_child(test_root)
	camera = Camera3D.new()
	camera.current = true
	test_root.add_child(camera)
	georeference = CesiumGeoreference.new()
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	test_root.add_child(georeference)

	var requested_case := OS.get_environment("CESIUM_TEST_LOD_CASE")
	var cases := [
		["replace", "REPLACE", false, false],
		["add", "ADD", false, false],
		["replace-transition", "REPLACE", true, false],
		["replace-transition-unsupported", "REPLACE", true, true],
	]
	var repetitions := maxi(
		1,
		OS.get_environment("CESIUM_TEST_LOD_CASE_REPETITIONS").to_int()
	)
	for test_case in cases:
		if not requested_case.is_empty() and requested_case != test_case[0]:
			continue
		for repetition in range(repetitions):
			print(
				"Running LOD continuity case: %s (%d/%d)"
				% [test_case[0], repetition + 1, repetitions]
			)
			if not await _run_refinement_case(test_case[1], test_case[2], test_case[3]):
				return

	print("Cesium ADD/REPLACE continuity and dithered LOD transition tests passed")
	test_root.free()
	for _frame in range(4):
		await process_frame
	quit(0)


func _run_refinement_case(
	refinement: String,
	use_transitions := false,
	use_unsupported_material := false
) -> bool:
	_move_camera_far()
	var case_name := "%s%s" % [
		refinement.to_lower(),
		(
			"-transition-unsupported"
			if use_unsupported_material else
			"-transition" if use_transitions else ""
		),
	]
	var token := "lod-%s-%d" % [case_name, Time.get_ticks_usec()]
	var tileset_url := _write_lod_tileset(refinement, token)
	if tileset_url.is_empty():
		_fail("Could not write the %s LOD fixture" % refinement)
		return false

	var receiver := TrackingReceiver.new()
	receiver.use_unsupported_material = use_unsupported_material
	receiver.name = "%sReceiver" % case_name.capitalize()
	test_root.add_child(receiver)
	var tileset := Cesium3DTileset.new()
	tileset.name = "%sTileset" % case_name.capitalize()
	tileset.data_source = 1
	tileset.url = tileset_url
	tileset.create_physics_meshes = false
	# Begin with a deliberately permissive threshold so only the coarse parent
	# is selected. Tighten it live after moving near to force deterministic
	# refinement without depending on the headless viewport's exact projection.
	tileset.maximum_screen_space_error = 1000.0
	tileset.maximum_simultaneous_tile_loads = 2
	tileset.preload_ancestors = true
	tileset.preload_siblings = true
	tileset.loading_descendant_limit = 7
	tileset.forbid_holes = true
	tileset.movement_prediction_enabled = use_transitions
	tileset.movement_prediction_minimum_speed = 0.0
	tileset.lod_transitions_enabled = use_transitions
	tileset.lod_transition_length = transition_length
	tileset.kick_descendants_while_fading_in = true
	tileset.http_cache_enabled = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)

	var parent_id := ""
	for _frame in range(FRAME_TIMEOUT):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		parent_id = _first_visible_id(receiver)
		if not parent_id.is_empty():
			break
	if parent_id.is_empty():
		_fail("%s parent did not become visible at coarse LOD" % refinement)
		return false
	if _marker_contains(token):
		_fail("%s child was requested before its screen error required refinement" % refinement)
		return false
	if use_transitions:
		var initial_transition_finished := false
		for _frame in range(FRAME_TIMEOUT):
			tileset.update_tileset(camera.global_transform)
			await process_frame
			if int(tileset.get_streaming_statistics().lod_transition_active_tiles) == 0:
				initial_transition_finished = true
				break
		if not initial_transition_finished:
			_fail("Initial generated-material LOD transition did not finish")
			return false

	var settings := tileset.get_streaming_statistics()
	if (
		not is_equal_approx(float(settings.maximum_screen_space_error), 1000.0) or
		not bool(settings.preload_ancestors) or
		not bool(settings.preload_siblings) or
		int(settings.loading_descendant_limit) != 7 or
		not bool(settings.forbid_holes) or
		bool(settings.lod_transitions_enabled) != use_transitions or
		not is_equal_approx(
			float(settings.lod_transition_length),
			transition_length
		) or
		not bool(settings.kick_descendants_while_fading_in)
	):
		_fail("LOD controls were not applied live: %s" % [settings])
		return false

	_move_camera_near()
	tileset.maximum_screen_space_error = 1.0
	var child_request_started := false
	for _frame in range(FRAME_TIMEOUT):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		if _marker_contains(token):
			child_request_started = true
			break
	if not child_request_started:
		_fail("%s child request did not start after refinement" % refinement)
		return false
	if not bool(receiver.visible_by_id.get(parent_id, false)):
		_fail("%s parent disappeared while its child was still downloading" % refinement)
		return false

	var child_id := ""
	var revealed_hole := false
	for _frame in range(FRAME_TIMEOUT):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		child_id = _first_visible_id_except(receiver, parent_id)
		if (
			child_id.is_empty() and
			not bool(receiver.visible_by_id.get(parent_id, false))
		):
			revealed_hole = true
			break
		if not child_id.is_empty():
			break
	if revealed_hole or child_id.is_empty():
		_fail(
			"%s refinement revealed a blank frame or never published its child: %s"
			% [refinement, receiver.events]
		)
		return false

	# The Godot adapter publishes tiles-to-render before hiding fading parents in
	# the same update. Check the stable end-of-frame state, which is what renders.
	var parent_visible := bool(receiver.visible_by_id.get(parent_id, false))
	if refinement == "REPLACE" and use_transitions:
		if not parent_visible:
			_fail("Dithered REPLACE hid its parent as soon as the child appeared")
			return false
		if not await _wait_for_transition_completion(
			tileset,
			receiver,
			parent_id,
			child_id,
			not use_unsupported_material
		):
			return false
		parent_visible = bool(receiver.visible_by_id.get(parent_id, false))
	elif refinement == "REPLACE" and parent_visible:
		_fail("REPLACE retained its coarse parent after the child was ready")
		return false
	if refinement == "ADD" and not parent_visible:
		_fail("ADD removed its parent after publishing the additive child")
		return false

	tileset.free()
	await process_frame
	receiver.free()
	await process_frame
	return true


func _wait_for_transition_completion(
	tileset: Cesium3DTileset,
	receiver: TrackingReceiver,
	parent_id: String,
	child_id: String,
	expect_supported_material: bool
) -> bool:
	var saw_fading_parent := false
	var saw_supported_material := false
	var saw_unsupported_material := false
	var saw_intermediate_percentage := false
	var saw_transition_state := false
	var saw_isolated_material := false
	var saw_shared_shader := false
	var saw_parameter_sync := false
	var saw_complementary_shader := false
	var saw_prediction_suppressed := false
	for _frame in range(FRAME_TIMEOUT):
		tileset.update_tileset(camera.global_transform)
		await process_frame
		var stats := tileset.get_streaming_statistics()
		saw_prediction_suppressed = (
			saw_prediction_suppressed or
			bool(stats.movement_prediction_suppressed_by_lod_transitions)
		)
		saw_fading_parent = saw_fading_parent or int(stats.fading_out) > 0
		saw_supported_material = (
			saw_supported_material or
			int(stats.lod_transition_supported_primitives) > 0
		)
		saw_unsupported_material = (
			saw_unsupported_material or
			int(stats.lod_transition_unsupported_primitives) > 0
		)
		var minimum_percentage := float(stats.lod_transition_minimum_percentage)
		var maximum_percentage := float(stats.lod_transition_maximum_percentage)
		saw_intermediate_percentage = (
			saw_intermediate_percentage or
			(minimum_percentage > 0.0 and minimum_percentage < 1.0) or
			(maximum_percentage > 0.0 and maximum_percentage < 1.0)
		)
		for tile_id in [parent_id, child_id]:
			var tile := receiver.tiles_by_id.get(tile_id) as Cesium3DTile
			if tile == null or tile.get_loaded_tile_primitive_count() < 1:
				continue
			var primitive := tile.get_loaded_tile_primitive(0)
			var render_node := primitive.render_node if primitive != null else null
			var active_material := (
				primitive.active_material as ShaderMaterial
				if primitive != null else null
			)
			var base_material := (
				primitive.selected_base_material as ShaderMaterial
				if primitive != null else null
			)
			if (
				active_material != null and
				base_material != null and
				active_material != base_material
			):
				saw_isolated_material = true
				saw_shared_shader = (
					saw_shared_shader or
					active_material.shader == base_material.shader
				)
				var shader_code := active_material.shader.code
				saw_complementary_shader = (
					saw_complementary_shader or (
						shader_code.contains("dither < threshold") and
						shader_code.contains("dither >= threshold")
					)
				)
			if (
				render_node != null and
				render_node.has_meta("cesium_lod_transition_percentage")
			):
				saw_transition_state = true
				if active_material != null:
					saw_parameter_sync = (
						saw_parameter_sync or (
							is_equal_approx(
								float(active_material.get_shader_parameter(
									"cesium_lod_transition_percentage"
								)),
								float(render_node.get_meta(
									"cesium_lod_transition_percentage"
								))
							) and
							bool(active_material.get_shader_parameter(
								"cesium_lod_fading_out"
							)) == bool(render_node.get_meta(
								"cesium_lod_fading_out"
							))
						)
					)
		if not bool(receiver.visible_by_id.get(parent_id, true)):
			var common_evidence_missing := (
				not saw_fading_parent or
				not saw_intermediate_percentage or
				not saw_prediction_suppressed
			)
			var supported_evidence_missing := (
				expect_supported_material and (
					not saw_supported_material or
					not saw_transition_state or
					not saw_isolated_material or
					not saw_shared_shader or
					not saw_parameter_sync or
					not saw_complementary_shader or
					saw_unsupported_material
				)
			)
			var unsupported_evidence_missing := (
				not expect_supported_material and (
					not saw_unsupported_material or
					saw_supported_material or
					saw_transition_state or
					saw_isolated_material
				)
			)
			if (
				common_evidence_missing or
				supported_evidence_missing or
				unsupported_evidence_missing
			):
				_fail(
					"LOD transition completed without complete dither evidence: %s"
					% [stats]
				)
				return false
			return true
	_fail("Dithered REPLACE parent did not finish fading out")
	return false


func _first_visible_id(receiver: TrackingReceiver) -> String:
	for key in receiver.visible_by_id:
		if bool(receiver.visible_by_id[key]):
			return str(key)
	return ""


func _first_visible_id_except(
	receiver: TrackingReceiver,
	excluded_id: String
) -> String:
	for key in receiver.visible_by_id:
		if str(key) != excluded_id and bool(receiver.visible_by_id[key]):
			return str(key)
	return ""


func _marker_contains(token: String) -> bool:
	return (
		FileAccess.file_exists(marker_path) and
		FileAccess.get_file_as_string(marker_path).contains(token)
	)


func _move_camera_far() -> void:
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 8000.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)


func _move_camera_near() -> void:
	camera.position = Vector3(EARTH_RADIUS + 50.0, 100.0, 300.0)
	camera.look_at(Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0), Vector3.UP)


func _write_lod_tileset(refinement: String, token: String) -> String:
	var output_directory := ProjectSettings.globalize_path("res://.test-generated")
	DirAccess.make_dir_recursive_absolute(output_directory)
	var parent_content := "file://" + ProjectSettings.globalize_path(
		"res://fixtures/lifecycle/triangle.gltf"
	)
	var child_content := server_url + "/slow-content.gltf?token=" + token
	var definition := {
		"asset": {"version": "1.1"},
		"geometricError": 1000,
		"root": {
			"transform": [
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				EARTH_RADIUS, 0, 0, 1,
			],
			"boundingVolume": {
				"box": [
					50, 0, 50,
					100, 0, 0,
					0, 100, 0,
					0, 0, 100,
				]
			},
			"geometricError": 1000,
			"refine": refinement,
			"content": {"uri": parent_content},
			"children": [{
				"boundingVolume": {
					"box": [
						50, 0, 50,
						100, 0, 0,
						0, 100, 0,
						0, 0, 100,
					]
				},
				"geometricError": 0,
				"content": {"uri": child_content},
			}],
		},
	}
	var output_path := output_directory.path_join(
		"lod-%s.json" % token.validate_filename()
	)
	var output := FileAccess.open(output_path, FileAccess.WRITE)
	if output == null:
		return ""
	output.store_string(JSON.stringify(definition, "\t"))
	output.close()
	return "file://" + output_path


func _fail(message: String) -> void:
	push_error(message)
	if is_instance_valid(test_root):
		test_root.free()
	quit(1)
