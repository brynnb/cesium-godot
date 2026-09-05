extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	for entry in [
		["CesiumGeoreference", "ellipsoid"],
		["Cesium3DTileset", "point_cloud_shading"],
		["CesiumGeoJsonDocumentRasterOverlay", "default_style"],
	]:
		if ClassDB.class_get_property_default_value(entry[0], entry[1]) != null:
			push_error("Constructor retained an instantiated resource default: " + entry[0])
			quit(1)
			return
		var first = ClassDB.instantiate(entry[0])
		var second = ClassDB.instantiate(entry[0])
		root.add_child(first)
		root.add_child(second)
		if first.get(entry[1]) == null or first.get(entry[1]) == second.get(entry[1]):
			push_error("Scene instances must own independent initialized resources")
			quit(1)
			return
		first.get(entry[1]).set_meta("roundtrip", 123)
		var packed := PackedScene.new()
		if packed.pack(first) != OK:
			push_error("Could not serialize resource owner")
			quit(1)
			return
		var restored = packed.instantiate()
		root.add_child(restored)
		if restored.get(entry[1]).get_meta("roundtrip", 0) != 123:
			push_error("Configured resource did not survive scene serialization")
			quit(1)
			return
		restored.free()
		first.free()
		second.free()
	print("Cesium editor resource defaults test passed")
	quit()
