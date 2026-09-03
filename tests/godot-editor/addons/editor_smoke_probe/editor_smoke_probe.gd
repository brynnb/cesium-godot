@tool
extends EditorPlugin

const SUCCESS_MARKER := "Cesium editor dock registration test passed"
const MAXIMUM_FRAMES := 1200

var frames_waited := 0


func _enter_tree() -> void:
	set_process(true)


func _process(_delta: float) -> void:
	frames_waited += 1
	var editor_root := get_editor_interface().get_base_control()
	var dock := editor_root.find_child("Cesium Ion Panel", true, false) as Control
	if dock != null and not get_editor_interface().get_resource_filesystem().is_scanning():
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
		print(SUCCESS_MARKER)
		get_tree().quit(0)
		return

	if frames_waited >= MAXIMUM_FRAMES:
		_fail("Cesium editor plugin did not register its dock")


func _fail(message: String) -> void:
	push_error(message)
	get_tree().quit(1)
