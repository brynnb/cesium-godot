extends Cesium3DTilesetLifecycleEventReceiver

var events: Array[String] = []


func _on_tile_loaded(_tile: Cesium3DTile) -> void:
	events.append("tile_loaded")


func _on_tile_visibility_changed(_tile: Cesium3DTile, visible: bool) -> void:
	events.append("visibility:%s" % visible)


func _on_tile_unloading(tile: Cesium3DTile) -> void:
	if not is_instance_valid(tile):
		events.append("invalid_tile_unloading")
	else:
		events.append("tile_unloading")
