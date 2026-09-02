extends Node3D

const DemoReceiver := preload("res://demo_receiver.gd")
const StreamingBenchmark := preload(
	"res://cesium_streaming_benchmark.gd"
)
const EARTH_RADIUS := 6_378_137.0
const HOME_POSITION := Vector3(EARTH_RADIUS + 50.0, 115.0, 330.0)
const HOME_TARGET := Vector3(EARTH_RADIUS + 50.0, 0.0, 50.0)
const AWAY_POSITION := Vector3(1_000_000.0, 1_000_000.0, 1_000_000.0)
const AWAY_TARGET := Vector3(2_000_000.0, 1_000_000.0, 1_000_000.0)
const SMOKE_TIMEOUT_MILLISECONDS := 15_000

var camera: Camera3D
var georeference: CesiumGeoreference
var tileset: Cesium3DTileset
var receiver: Cesium3DTilesetLifecycleEventReceiver
var current_primitive: CesiumLoadedTilePrimitive
var overlay_texture: ImageTexture
var overlay_attached := false
var camera_away := false
var event_lines: Array[String] = []
var event_log: RichTextLabel
var status_label: Label
var overlay_button: Button
var view_button: Button
var smoke_mode := false
var smoke_deadline := 0
var smoke_cleanup_started := false
var benchmark_mode := false
var benchmark_started := false


func _ready() -> void:
	smoke_mode = OS.get_cmdline_user_args().has("--smoke-test")
	benchmark_mode = OS.get_cmdline_user_args().has("--benchmark-route")
	smoke_deadline = Time.get_ticks_msec() + SMOKE_TIMEOUT_MILLISECONDS
	_create_camera()
	_create_georeference()
	_create_receiver()
	_create_interface()
	_create_tileset()
	if benchmark_mode:
		_run_benchmark_route.call_deferred()


func _process(_delta: float) -> void:
	if benchmark_mode:
		return
	if tileset != null and is_instance_valid(tileset):
		tileset.update_tileset(camera.global_transform)
		_update_status()
	if smoke_mode:
		_run_smoke_sequence()


func _create_camera() -> void:
	camera = Camera3D.new()
	camera.name = "DemoCamera"
	camera.current = true
	camera.fov = 55.0
	add_child(camera)
	_move_camera(false)


func _create_georeference() -> void:
	georeference = CesiumGeoreference.new()
	georeference.name = "LocalCesiumCoordinates"
	georeference.origin_type = CesiumGeoreference.TrueOrigin
	add_child(georeference)


func _create_receiver() -> void:
	receiver = DemoReceiver.new()
	receiver.name = "LifecycleMaterialReceiver"
	receiver.event_recorded.connect(_on_event_recorded)
	receiver.primitive_ready.connect(_on_primitive_ready)
	receiver.tile_ready.connect(_on_tile_ready)
	add_child(receiver)


func _create_tileset() -> void:
	current_primitive = null
	overlay_attached = false
	tileset = Cesium3DTileset.new()
	tileset.name = "LocalDemoTileset"
	tileset.data_source = 1
	tileset.url = "file://" + ProjectSettings.globalize_path(
		"res://fixtures/tileset.json"
	)
	tileset.create_physics_meshes = false
	tileset.http_cache_enabled = false
	tileset.maximum_screen_space_error = 1.0
	tileset.maximum_simultaneous_tile_loads = 4
	tileset.preload_ancestors = false
	tileset.preload_siblings = false
	tileset.lifecycle_event_receiver = receiver
	georeference.add_child(tileset)


func _create_interface() -> void:
	var panel := PanelContainer.new()
	panel.position = Vector2(16.0, 16.0)
	panel.size = Vector2(515.0, 650.0)
	add_child(panel)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 14)
	margin.add_theme_constant_override("margin_top", 12)
	margin.add_theme_constant_override("margin_right", 14)
	margin.add_theme_constant_override("margin_bottom", 12)
	panel.add_child(margin)

	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 8)
	margin.add_child(column)

	var title := Label.new()
	title.text = "Cesium lifecycle + material integration"
	title.add_theme_font_size_override("font_size", 22)
	column.add_child(title)

	var explanation := Label.new()
	explanation.text = (
		"This project streams one local 3D Tile with no network or Vanguard code.\n"
		+ "The log below is produced by the real receiver hooks."
	)
	column.add_child(explanation)

	var controls := HBoxContainer.new()
	controls.add_theme_constant_override("separation", 6)
	column.add_child(controls)

	overlay_button = Button.new()
	overlay_button.text = "Detach overlay"
	overlay_button.pressed.connect(_toggle_overlay)
	controls.add_child(overlay_button)

	view_button = Button.new()
	view_button.text = "Look away"
	view_button.pressed.connect(_toggle_view)
	controls.add_child(view_button)

	var recreate_button := Button.new()
	recreate_button.text = "Unload + reload"
	recreate_button.pressed.connect(_recreate_tileset)
	controls.add_child(recreate_button)

	status_label = Label.new()
	status_label.text = "Waiting for local tile..."
	column.add_child(status_label)

	event_log = RichTextLabel.new()
	event_log.custom_minimum_size = Vector2(480.0, 475.0)
	event_log.fit_content = false
	event_log.scroll_active = true
	event_log.bbcode_enabled = true
	column.add_child(event_log)


func _on_event_recorded(message: String) -> void:
	event_lines.append(message)
	if event_lines.size() > 80:
		event_lines.pop_front()
	if event_log != null:
		event_log.text = "[font_size=13]%s[/font_size]" % "\n".join(event_lines)
		event_log.scroll_to_line(max(0, event_lines.size() - 1))


func _on_primitive_ready(primitive: CesiumLoadedTilePrimitive) -> void:
	current_primitive = primitive


func _on_tile_ready(_tile: Cesium3DTile) -> void:
	_attach_overlay.call_deferred()


func _attach_overlay() -> void:
	if (
		overlay_attached or current_primitive == null or
		current_primitive.loaded_tile == null or tileset == null
	):
		return
	if overlay_texture == null:
		overlay_texture = _make_checker_texture()
	var binding := tileset.attach_raster_overlay(
		current_primitive,
		"Demo Overlay",
		overlay_texture,
		0,
		current_primitive.get_overlay_texture_coordinate_index(0),
		Vector2.ZERO,
		Vector2.ONE
	)
	overlay_attached = binding != null
	if overlay_button != null:
		overlay_button.text = "Detach overlay" if overlay_attached else "Attach overlay"


func _detach_overlay() -> void:
	if not overlay_attached or current_primitive == null or tileset == null:
		return
	overlay_attached = not tileset.detach_raster_overlay(
		current_primitive,
		"Demo Overlay",
		overlay_texture
	)
	if overlay_button != null:
		overlay_button.text = "Detach overlay" if overlay_attached else "Attach overlay"


func _toggle_overlay() -> void:
	if overlay_attached:
		_detach_overlay()
	else:
		_attach_overlay()


func _toggle_view() -> void:
	camera_away = not camera_away
	_move_camera(camera_away)
	view_button.text = "Look at tile" if camera_away else "Look away"


func _move_camera(away: bool) -> void:
	camera.position = AWAY_POSITION if away else HOME_POSITION
	camera.look_at(AWAY_TARGET if away else HOME_TARGET, Vector3.UP)


func _recreate_tileset() -> void:
	if tileset != null and is_instance_valid(tileset):
		tileset.free()
	tileset = null
	current_primitive = null
	overlay_attached = false
	_create_tileset.call_deferred()


func _update_status() -> void:
	if status_label == null:
		return
	var statistics := tileset.get_streaming_statistics()
	status_label.text = (
		"selected=%d  loaded=%d  visible=%d  worker=%d  main=%d"
		% [
			int(statistics.get("selected", 0)),
			int(statistics.get("loaded", 0)),
			int(statistics.get("visible", 0)),
			int(statistics.get("worker_queue", 0)),
			int(statistics.get("main_thread_queue", 0)),
		]
	)


func _run_smoke_sequence() -> void:
	if Time.get_ticks_msec() > smoke_deadline:
		_smoke_fail("timed out; callbacks=%s" % receiver.hook_counts)
		return
	if not smoke_cleanup_started:
		if int(receiver.hook_counts.get("raster_overlay_attached", 0)) < 1:
			return
		_detach_overlay()
		if tileset != null:
			tileset.free()
		tileset = null
		smoke_cleanup_started = true
		return

	var required_callbacks := [
		"material_selector",
		"material_customizing",
		"tile_mesh_primitive_loaded",
		"raster_overlay_attached",
		"raster_overlay_detaching",
		"tile_loaded",
		"tile_visibility_changed",
		"tile_unloading",
	]
	for callback_name in required_callbacks:
		if int(receiver.hook_counts.get(callback_name, 0)) < 1:
			return
	print("Cesium lifecycle/material example smoke test passed: ", receiver.hook_counts)
	get_tree().quit(0)


func _run_benchmark_route() -> void:
	if benchmark_started:
		return
	benchmark_started = true
	var benchmark := StreamingBenchmark.new()
	var route: Array[Dictionary] = [
		{
			"position": HOME_POSITION,
			"target": HOME_TARGET,
			"travel_frames": 1,
			"settle_frames": 30,
		},
		{
			"position": HOME_POSITION + Vector3(0.0, 0.0, -180.0),
			"target": HOME_TARGET,
			"travel_frames": 20,
			"settle_frames": 10,
		},
		{
			"position": AWAY_POSITION,
			"target": AWAY_TARGET,
			"travel_frames": 6,
			"settle_frames": 10,
		},
		{
			"position": HOME_POSITION,
			"target": HOME_TARGET,
			"travel_frames": 6,
			"settle_frames": 30,
		},
	]
	var result: Dictionary = await benchmark.run(tileset, camera, route)
	result["fixture"] = "lifecycle_material_demo"
	if (
		result.has("error") or
		str(result.get("schema", "")) != "cesium-godot-benchmark-v1" or
		int(result.get("route_frames", 0)) != 113 or
		int(result.get("maximum_loaded_data_bytes", 0)) <= 0 or
		int(result.get("visual_transition_frames", 0)) <= 0 or
		int(result.get("terminal_failures", -1)) != 0
	):
		_smoke_fail("deterministic benchmark route failed: %s" % [result])
		return
	var encoded := JSON.stringify(result)
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--benchmark-output="):
			var output_path := argument.trim_prefix("--benchmark-output=")
			var output := FileAccess.open(output_path, FileAccess.WRITE)
			if output == null:
				_smoke_fail("could not open benchmark output: %s" % output_path)
				return
			output.store_string(encoded + "\n")
			output.close()
			break
	print("CESIUM_BENCHMARK_RESULT ", encoded)
	if tileset != null:
		tileset.free()
		tileset = null
	await get_tree().process_frame
	get_tree().quit(0)


func _smoke_fail(message: String) -> void:
	push_error("Cesium lifecycle/material example smoke test failed: " + message)
	get_tree().quit(1)


func _make_checker_texture() -> ImageTexture:
	var image := Image.create(32, 32, true, Image.FORMAT_RGBA8)
	for y in range(32):
		for x in range(32):
			var alternate := (int(x / 4) + int(y / 4)) % 2 == 0
			image.set_pixel(
				x,
				y,
				Color(1.0, 0.72, 0.15, 1.0) if alternate
				else Color(0.08, 0.18, 0.42, 1.0)
			)
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)
