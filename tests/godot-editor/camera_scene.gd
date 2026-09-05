extends Node3D

var smoke_complete := false
var started := Time.get_ticks_msec()

func _ready() -> void:
	# The local triangle fixture is single-sided; view its authored front face.
	$Camera3D.position.y = -115
	var lighting := WorldEnvironment.new()
	lighting.environment = Environment.new()
	lighting.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	lighting.environment.ambient_light_color = Color.WHITE
	lighting.environment.ambient_light_energy = 1.0
	add_child(lighting)
	if not has_node("CesiumStreaming"):
		var driver := Node.new()
		driver.set_script(load("res://addons/cesium_godot/scripts/cesium_scene_streaming.gd"))
		add_child(driver)
	$Camera3D.look_at(Vector3(6378187, 0, 50))
	$Georeference/Tileset.url = CesiumUrlUtility.local_path_to_file_url(
		"res://../../examples/lifecycle_material_demo/fixtures/tileset.json"
	)

func _process(_delta: float) -> void:
	if smoke_complete:
		return
	if OS.get_cmdline_user_args().has("--smoke-test"):
		if $Georeference/Tileset.get_streaming_statistics().get("visible", 0) > 0:
			smoke_complete = true
			if OS.get_cmdline_user_args().has("--capture"):
				# Selection precedes renderer preparation and any LOD fade-in.
				await get_tree().create_timer(2.0).timeout
				await RenderingServer.frame_post_draw
				var image := get_viewport().get_texture().get_image()
				image.save_png("res://runtime.png")
				var background := image.get_pixel(0, 0)
				var different := 0
				for y in range(0, image.get_height(), 8):
					for x in range(0, image.get_width(), 8):
						if image.get_pixel(x, y) != background:
							different += 1
				if different < 25:
					push_error("Rendered fixture is blank despite tile selection")
					get_tree().quit(1)
					return
			$Georeference/Tileset.free()
			await get_tree().process_frame
			print("Ordinary Camera3D streaming fixture passed")
			get_tree().quit()
		elif Time.get_ticks_msec() - started > 15000:
			push_error("Ordinary Camera3D failed to select local tile")
			get_tree().quit(1)
