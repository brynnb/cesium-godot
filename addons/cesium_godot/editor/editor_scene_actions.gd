@tool
extends RefCounted
## All dock scene mutations share one undo transaction and explicit ownership.

var plugin: EditorPlugin
const DRIVER = preload("res://addons/cesium_godot/scripts/cesium_scene_streaming.gd")

func _init(editor_plugin: EditorPlugin) -> void:
	plugin = editor_plugin

func add_asset(asset_name: String, source: String = "", ion_id: int = 0, asset_type: String = "3DTILES") -> void:
	var root := plugin.get_editor_interface().get_edited_scene_root()
	if root == null:
		push_warning("Create or open a scene before adding a Cesium asset.")
		return
	if not _unambiguous(root, ["CesiumGeoreference", "CesiumCameraManager"]):
		return
	var undo := plugin.get_undo_redo()
	undo.create_action("Add Cesium asset", UndoRedo.MERGE_DISABLE, root)
	var globe := _find(root, "CesiumGeoreference")
	if globe == null:
		globe = CesiumGeoreference.new()
		globe.name = "CesiumGeoreference"
		_add(undo, root, globe, root)
	var tile := Cesium3DTileset.new()
	tile.name = asset_name.validate_node_name()
	if not source.is_empty():
		tile.data_source = 1
		tile.url = source
	elif ion_id > 0:
		tile.ion_asset_id = 1 if asset_type == "IMAGERY" else ion_id
	_add(undo, globe, tile, root)
	var manager := _find(root, "CesiumCameraManager")
	if manager != null:
		undo.add_do_method(self, "_wire_manager", tile, manager)
	if asset_type == "IMAGERY":
		var overlay := CesiumIonRasterOverlay.new()
		overlay.name = "Imagery"
		overlay.asset_id = ion_id
		_add(undo, tile, overlay, root)
	undo.commit_action()
	plugin.get_editor_interface().get_selection().clear()
	plugin.get_editor_interface().get_selection().add_node(tile)

func setup_scene(camera_script: Script = null) -> void:
	var root := plugin.get_editor_interface().get_edited_scene_root()
	if root == null:
		push_warning("Create or open a scene first.")
		return
	if not _unambiguous(root, ["CesiumGeoreference", "CesiumCameraManager", "Camera3D"]):
		return
	var undo := plugin.get_undo_redo()
	undo.create_action("Set up Cesium camera", UndoRedo.MERGE_DISABLE, root)
	var globe := _find(root, "CesiumGeoreference")
	if globe == null:
		globe = CesiumGeoreference.new()
		globe.name = "CesiumGeoreference"
		_add(undo, root, globe, root)
	var camera := _find(root, "Camera3D") as Camera3D
	var new_camera := camera == null
	if new_camera:
		camera = Camera3D.new()
		camera.name = "CesiumCamera"
		camera.position = Vector3(0, 1000, 2000)
		camera.rotation_degrees.x = -25
		camera.far = 1000000
		camera.current = true
	if camera_script != null and new_camera:
		camera.set_script(camera_script)
		camera.set("globe_node", globe)
		if camera_script.resource_path.ends_with("CesiumOrbitCamera.gd"):
			camera.set("target", globe)
		var tiles: Array[Cesium3DTileset] = []
		for tile in root.find_children("*", "Cesium3DTileset", true, false):
			tiles.append(tile)
		camera.set("tilesets", tiles)
	if new_camera:
		_add(undo, root, camera, root)
	var has_driver: bool = root.get_script() == DRIVER
	for node in root.find_children("*", "Node", true, false):
		has_driver = has_driver or node.get_script() == DRIVER
	if not camera.has_method("_update_tilesets") and not has_driver:
		var driver := Node.new()
		driver.name = "CesiumStreaming"
		driver.set_script(DRIVER)
		_add(undo, root, driver, root)
	var manager := _find(root, "CesiumCameraManager")
	if manager == null:
		manager = CesiumCameraManager.new()
		manager.name = "CesiumCameraManager"
		manager.use_viewport_camera = true
		_add(undo, root, manager, root)
	for tile in root.find_children("*", "Cesium3DTileset", true, false):
		undo.add_do_method(self, "_wire_manager", tile, manager)
		undo.add_undo_property(tile, "camera_manager_path", tile.camera_manager_path)
	undo.commit_action()

func _wire_manager(tile: Cesium3DTileset, manager: Node) -> void:
	tile.camera_manager_path = tile.get_path_to(manager)

func _find(root: Node, type: String) -> Node:
	if root.is_class(type):
		return root
	var nodes := root.find_children("*", type, true, false)
	return nodes[0] if nodes.size() == 1 else null

func _unambiguous(root: Node, types: Array) -> bool:
	for type in types:
		var count := root.find_children("*", type, true, false).size() + int(root.is_class(type))
		if count > 1:
			push_warning("Cesium setup found multiple %s nodes. Configure this scene manually; no changes were made." % type)
			return false
	return true

func _add(undo: EditorUndoRedoManager, parent: Node, child: Node, owner_node: Node) -> void:
	undo.add_do_method(parent, "add_child", child, true)
	undo.add_do_method(child, "set_owner", owner_node)
	undo.add_do_reference(child)
	undo.add_undo_method(parent, "remove_child", child)
