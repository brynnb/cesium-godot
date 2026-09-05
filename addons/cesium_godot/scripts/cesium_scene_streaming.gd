extends Node
## Optional scene driver for the editor's ordinary-camera setup. Applications
## with their own update loop should remove this node, not drive tilesets twice.

func _process(_delta: float) -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	if camera.has_method("_update_tilesets"):
		return # Optional Cesium camera controllers already own this update loop.
	for tileset in get_parent().find_children("*", "Cesium3DTileset", true, false):
		tileset.update_tileset(camera.global_transform)
