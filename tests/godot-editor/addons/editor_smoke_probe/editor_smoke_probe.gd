@tool
extends EditorPlugin

const SUCCESS_MARKER := "Cesium editor dock registration test passed"
const MAXIMUM_FRAMES := 1200

var frames_waited := 0
var checking_cleanup := false


func _enter_tree() -> void:
	set_process(true)


func _process(_delta: float) -> void:
	frames_waited += 1
	var editor_root := get_editor_interface().get_base_control()
	var dock := editor_root.find_child("Cesium Ion Panel", true, false) as Control
	if dock != null and not get_editor_interface().get_resource_filesystem().is_scanning():
		if checking_cleanup:
			return
		checking_cleanup = true
		for control_name in [
			"BlankTilesetButton",
			"DynamicCameraButton",
			"OrbitCameraButton",
			"ConnectButton",
		]:
			var control := dock.find_child(control_name, true, false)
			if not control is Button:
				_fail("Cesium dock is missing expected Button: %s" % control_name)
				return
		_check_scene_and_cleanup.call_deferred(dock)
		return

	if frames_waited >= MAXIMUM_FRAMES:
		_fail("Cesium editor plugin did not register its dock")


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)

func _check_scene_and_cleanup(dock: Control) -> void:
	get_editor_interface().open_scene_from_path("res://camera_scene.tscn")
	await get_tree().process_frame
	var scene := get_editor_interface().get_edited_scene_root()
	if scene == null or not scene.get_node_or_null("Camera3D") is Camera3D:
		_fail("Ordinary Camera3D fixture did not open")
		return
	var tileset := scene.get_node("Georeference/Tileset")
	if tileset.get_node(tileset.camera_manager_path) != scene.get_node("CameraManager"):
		_fail("Ordinary camera selection is not wired to the tileset")
		return
	var actions = load("res://addons/cesium_godot/editor/editor_scene_actions.gd").new(self)
	var workspace = load("res://addons/cesium_godot/editor/editor_workspace.gd").new()
	add_child(workspace)
	workspace.initialize(self, actions)
	workspace.ion_entries = [{"name": "Building", "id": 1, "type": "3DTILES"}, {"name": "Imagery", "id": 2, "type": "IMAGERY"}]
	workspace.search.text = "build"
	workspace.refresh_entries()
	if workspace.entries_box.get_child_count() != 1:
		_fail("Asset name search did not filter entries")
		return
	workspace.type_filter.select(3)
	workspace.refresh_entries()
	if workspace.entries_box.get_child_count() != 0:
		_fail("Asset type filter did not combine with search")
		return
	workspace.free()
	var old_token: Variant = ProjectSettings.get_setting("cesium/runtime/ion_access_token", null)
	ProjectSettings.set_setting("cesium/runtime/ion_access_token", "editor-test-placeholder")
	var config := CesiumGDConfig.new()
	add_child(config)
	if config.accessToken != "editor-test-placeholder":
		_fail("Runtime configuration did not read project token")
		return
	config.free()
	ProjectSettings.set_setting("cesium/runtime/ion_access_token", old_token)
	actions.add_asset("WorkflowTest", CesiumUrlUtility.local_path_to_file_url("res://../../examples/lifecycle_material_demo/fixtures/tileset.json"))
	var added := scene.get_node_or_null("Georeference/WorkflowTest")
	if added == null or added.owner != scene:
		_fail("Scene action did not create an owned tileset")
		return
	var history := get_undo_redo().get_history_undo_redo(get_undo_redo().get_object_history_id(scene))
	history.undo()
	if added.is_inside_tree():
		_fail("Undo did not remove added asset")
		return
	history.redo()
	if not added.is_inside_tree():
		_fail("Redo did not restore added asset")
		return
	actions.setup_scene()
	if scene.get_node_or_null("CesiumStreaming") == null or added.get_node(added.camera_manager_path) != scene.get_node("CameraManager"):
		_fail("Ordinary camera scene action did not wire streaming")
		return
	actions.setup_scene()
	if scene.find_children("*", "Camera3D", true, false).size() != 1 or scene.find_children("*", "CesiumGeoreference", true, false).size() != 1:
		_fail("Repeated setup duplicated existing renamed scene components")
		return
	history.undo() # Repeated setup, leaving the first setup intact.
	var packed := PackedScene.new()
	if packed.pack(scene) != OK or ResourceSaver.save(packed, "user://workflow.tscn") != OK:
		_fail("Could not save editor-created scene")
		return
	if OS.get_cmdline_user_args().has("--save-workflow"):
		if ResourceSaver.save(packed, "res://saved.tscn") != OK:
			_fail("Could not save packaged workflow scene")
			return
	var restored := (load("user://workflow.tscn") as PackedScene).instantiate()
	if restored.get_node_or_null("Georeference/WorkflowTest") == null or restored.get_node_or_null("CesiumStreaming") == null:
		_fail("Editor-created nodes were lost during scene serialization")
		return
	restored.free()
	history.undo()
	if scene.get_node_or_null("CesiumStreaming") != null:
		_fail("Scene setup undo did not remove driver")
		return
	history.undo()
	print("Cesium editor scene actions and serialization passed")
	var dock_reference := weakref(dock)
	var enabled_plugins: Variant = ProjectSettings.get_setting("editor_plugins/enabled")
	get_editor_interface().set_plugin_enabled("cesium_godot", false)
	await get_tree().process_frame
	await get_tree().process_frame
	if dock_reference.get_ref() != null:
		_fail("Disabling Cesium did not release its dock")
		return
	# Restore fixture configuration without restarting the plugin under test.
	ProjectSettings.set_setting("editor_plugins/enabled", enabled_plugins)
	ProjectSettings.save()
	print(SUCCESS_MARKER)
	get_tree().quit(0)
