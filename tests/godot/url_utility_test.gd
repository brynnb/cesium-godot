extends SceneTree


func _init() -> void:
	if CesiumUrlUtility.local_path_to_file_url("") != "":
		_fail("empty paths must remain empty")
		return
	if CesiumUrlUtility.local_path_to_file_url(
		"C:\\World Data\\Kōjan tileset.json"
	) != "file:///C:/World%20Data/K%C5%8Djan%20tileset.json":
		_fail("Windows drive paths were not canonicalized")
		return
	if CesiumUrlUtility.local_path_to_file_url(
		"\\\\server\\share name\\tileset.json"
	) != "file://server/share%20name/tileset.json":
		_fail("UNC paths were not canonicalized")
		return
	var fixture_url := CesiumUrlUtility.local_path_to_file_url(
		"res://fixtures/lifecycle/tileset.json"
	)
	if not fixture_url.begins_with("file:///"):
		_fail("Godot resource paths did not become absolute file URLs: %s" % fixture_url)
		return
	if not fixture_url.ends_with("/fixtures/lifecycle/tileset.json"):
		_fail("Godot resource path resolved to the wrong file: %s" % fixture_url)
		return
	print("Cesium cross-platform local-file URL tests passed")
	quit(0)


func _fail(message: String) -> void:
	push_error(message)
	quit(1)
