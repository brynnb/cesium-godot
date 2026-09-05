@tool
extends VBoxContainer
## Small dock workflow, using Godot theme controls and the shared catalog parser.
const Catalog = preload("res://addons/cesium_godot/editor/catalog/editor_asset_catalog.gd")
var actions: RefCounted
var plugin: EditorPlugin
var source: LineEdit
var search: LineEdit
var type_filter: OptionButton
var status: Label
var entries_box: VBoxContainer
var catalog := Catalog.new()
var ion_entries: Array = []
var elapsed := 0.0
var preview: CheckBox
var failures: Dictionary = {}

func initialize(editor_plugin: EditorPlugin, scene_actions: RefCounted) -> void:
	plugin = editor_plugin
	actions = scene_actions
	source = LineEdit.new()
	source.placeholder_text = "Tileset URL or local path"
	add_child(source)
	_button("Add tileset", _add_source)
	var picker := EditorFileDialog.new()
	picker.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILE
	picker.access = EditorFileDialog.ACCESS_FILESYSTEM
	picker.add_filter("*.json", "Tileset or asset catalog")
	add_child(picker)
	picker.file_selected.connect(_open_file)
	_button("Browse tileset / catalog…", func(): picker.popup_centered_ratio(0.65))
	_button("Set up ordinary camera", func(): actions.setup_scene())
	search = LineEdit.new()
	search.placeholder_text = "Search assets"
	add_child(search)
	search.text_changed.connect(func(_text): refresh_entries())
	type_filter = OptionButton.new()
	for label in ["All types", "3DTILES", "TERRAIN", "IMAGERY", "Local / catalog"]:
		type_filter.add_item(label)
	add_child(type_filter)
	type_filter.item_selected.connect(func(_index): refresh_entries())
	entries_box = VBoxContainer.new()
	add_child(entries_box)
	status = Label.new()
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	status.text = "Select a tileset for status. Press Play to stream the ordinary-camera scene."
	add_child(status)
	preview = CheckBox.new()
	preview.text = "Preview selected tileset (loads data)"
	add_child(preview)

func _button(title: String, callback: Callable) -> void:
	var button := Button.new()
	button.text = title
	button.pressed.connect(callback)
	add_child(button)

func _add_source() -> void:
	var result: Dictionary = Catalog.make_direct_entry("Tileset", source.text)
	if not result.ok:
		status.text = "Invalid source: " + "; ".join(result.errors)
		return
	actions.add_asset(result.entry.name, result.entry.url)

func _open_file(path: String) -> void:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		status.text = "Cannot open selected file."
		return
	var text := file.get_as_text()
	var parsed: Variant = JSON.parse_string(text)
	if parsed is Dictionary and parsed.get("schema", "") == "cesium-godot-asset-catalog-v1":
		if not catalog.parse_json(text, path):
			status.text = "; ".join(catalog.get_errors())
		refresh_entries()
	else:
		source.text = path
		_add_source()

func refresh_entries() -> void:
	for child in entries_box.get_children():
		entries_box.remove_child(child)
		child.queue_free()
	var query := search.text.to_lower()
	var filter := type_filter.get_item_text(type_filter.selected)
	for entry in ion_entries:
		if not query.is_empty() and not str(entry.get("name", "")).to_lower().contains(query):
			continue
		if filter != "All types" and filter != entry.get("type", ""):
			continue
		var button := Button.new()
		button.text = "Add %s (%s)" % [entry.get("name", "Unnamed"), entry.get("type", "")]
		button.disabled = entry.get("type", "") not in ["3DTILES", "TERRAIN", "IMAGERY"]
		button.pressed.connect(func(): actions.add_asset(str(entry.name), "", int(entry.id), str(entry.type)))
		entries_box.add_child(button)
	if filter in ["All types", "Local / catalog"]:
		for entry in catalog.get_entries(search.text):
			var button := Button.new()
			button.text = "Add " + str(entry.name)
			button.pressed.connect(func(): actions.add_asset(entry.name, entry.url))
			entries_box.add_child(button)

func _process(delta: float) -> void:
	if plugin == null:
		return
	var selected := plugin.get_editor_interface().get_selection().get_selected_nodes()
	if selected.size() != 1 or not selected[0] is Cesium3DTileset:
		return
	var tile := selected[0] as Cesium3DTileset
	var callback := _load_failed.bind(tile.get_instance_id())
	if not tile.load_failure.is_connected(callback):
		tile.load_failure.connect(callback)
	if preview.button_pressed:
		var camera := plugin.get_editor_interface().get_editor_viewport_3d(0).get_camera_3d()
		if camera != null:
			tile.update_tileset(camera.global_transform)
	elapsed += delta
	if elapsed < 0.5:
		return
	elapsed = 0
	var stats := tile.get_streaming_statistics()
	status.text = "%s: %.0f%% loaded, %d visible.\nEditor scene state (not the running game's remote tree)." % [tile.name, float(stats.get("load_progress_percent", 0)), int(stats.get("visible", 0))]
	if tile.data_source == 1 and tile.url.is_empty():
		status.text = "Not configured: enter a tileset URL."
	elif tile.data_source == 0 and tile.ion_access_token.is_empty() and str(ProjectSettings.get_setting("cesium/runtime/ion_access_token", "")).is_empty():
		status.text = "Needs runtime asset token. Editor sign-in alone does not configure Play."
	if failures.has(tile.get_instance_id()):
		status.text += "\nLast failure: " + str(failures[tile.get_instance_id()])

func _load_failed(failure: CesiumLoadFailure, instance_id: int) -> void:
	# Keep only the message, never a tile/resource reference or credential URL.
	failures[instance_id] = failure.message
