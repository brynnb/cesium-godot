@tool
extends EditorPlugin

class_name CesiumGodotEditorTool

const editorAddon := preload("res://addons/cesium_godot/panels/cesium_panel.tscn")

const token_panel_popup := preload("res://addons/cesium_godot/panels/token_panel.tscn")

const CESIUM_GLOBE_NAME = "CesiumGeoreference"
const CESIUM_TILESET_NAME = "Cesium3DTileset"

var docked_scene : Control

var add_button : Button
var upload_button : Button
var learn_button : Button
var help_button : Button
var token_button : Button
var sign_out_button : Button
var connect_button : Button
var connected_indicator : HBoxContainer
var blank_tileset_button : Button
var dynamic_camera_button : Button
var orbit_camera_button : Button
var ion_button_holder : Control

var ion_asset_buttons : Array[Button] = []

var ion_session : CesiumIonEditorSession = null
var cesium_builder_node : CesiumGDAssetBuilder = null
var inspector_plugin: EditorInspectorPlugin = null

# So, for some reason we cannot have a custom popup because some definitions get lost in instantiation
# We don't really know why this is, but we circunvent it by just storing the data on another class
var token_panel: Popup = null

var token_panel_data : TokenPanelData = null

func _enter_tree() -> void:
	self.set_process(false)
	self.docked_scene = editorAddon.instantiate()
	add_control_to_dock(EditorPlugin.DOCK_SLOT_RIGHT_UL, self.docked_scene)
	self.set_session_buttons_enabled(false)
	self.ion_session = CesiumIonEditorSession.new()
	self.ion_session.name = "CesiumIonEditorSession"
	self.ion_session.state_changed.connect(_on_ion_session_state_changed)
	self.ion_session.authorization_failed.connect(_on_ion_authorization_failed)
	self.add_child(self.ion_session)
	self.cesium_builder_node = CesiumGDAssetBuilder.new()
	self.add_child(self.cesium_builder_node)
	self.token_panel = self.token_panel_popup.instantiate()
	self.token_panel_data = TokenPanelData.new()
	self.add_child(self.token_panel)
	self.token_panel.hide()
	self.inspector_plugin = CesiumTooltips.new()
	self.add_inspector_plugin(self.inspector_plugin)
	print("Enabled Cesium plugin")
	self.init_buttons()


func _exit_tree() -> void:
	print("Disabled Cesium plugin")
	if self.inspector_plugin != null:
		remove_inspector_plugin(self.inspector_plugin)
		self.inspector_plugin = null
	if self.docked_scene != null:
		remove_control_from_docks(self.docked_scene)
		self.docked_scene.free()
		self.docked_scene = null

func init_buttons() -> void:
	self.add_button = self.docked_scene.find_child("AddButton") as Button
	self.upload_button = self.docked_scene.find_child("UploadButton") as Button
	self.token_button = self.docked_scene.find_child("TokenButton") as Button
	self.learn_button = self.docked_scene.find_child("LearnButton") as Button
	self.help_button = self.docked_scene.find_child("HelpButton") as Button
	self.sign_out_button = self.docked_scene.find_child("SignOutButton") as Button
	self.connect_button = self.docked_scene.find_child("ConnectButton") as Button
	self.connected_indicator = self.docked_scene.find_child("IONConnected") as HBoxContainer
	self.blank_tileset_button = self.docked_scene.find_child("BlankTilesetButton") as Button
	self.dynamic_camera_button = self.docked_scene.find_child("DynamicCameraButton") as Button
	self.orbit_camera_button = self.docked_scene.find_child("OrbitCameraButton") as Button
	self.token_panel_data.initialize_fields(self.token_panel, self.ion_session)
	self.apply_editor_icons()
	
	# Connect to their signals
	self.upload_button.pressed.connect(on_upload_pressed)
	self.learn_button.pressed.connect(on_learn_pressed)
	self.help_button.pressed.connect(on_help_pressed)
	self.connect_button.pressed.connect(on_connect_pressed)
	self.sign_out_button.pressed.connect(on_sign_out_pressed)
	self.blank_tileset_button.pressed.connect(add_tileset)
	self.dynamic_camera_button.pressed.connect(create_dynamic_camera)
	self.orbit_camera_button.pressed.connect(create_orbit_camera)
	self.token_button.pressed.connect(on_token_panel_pressed)
	
	self.docked_scene.find_child("VisitDepotButton").pressed.connect(on_visit_depot)
	self.add_ion_buttons()

func apply_editor_icons() -> void:
	# Editor-owned icons are available immediately, including on the first project
	# scan. Imported SVG dependencies can prevent the entire dock from registering.
	var editor_base := get_editor_interface().get_base_control()
	var icon_bindings := {
		self.upload_button: "Load",
		self.token_button: "Key",
		self.learn_button: "Info",
		self.help_button: "Help",
		self.sign_out_button: "Stop",
		self.blank_tileset_button: "BoxMesh",
		self.dynamic_camera_button: "Camera3D",
		self.orbit_camera_button: "Camera3D",
		self.connect_button: "Connect",
		self.docked_scene.find_child("VisitDepotButton"): "ExternalLink",
		self.token_panel.find_child("CloseButton"): "Close",
	}
	for control in icon_bindings:
		if control is Button:
			(control as Button).icon = editor_base.get_theme_icon(icon_bindings[control], "EditorIcons")

	var texture_bindings := {
		self.docked_scene.find_child("DynamicCameraIcon"): "Camera3D",
		self.docked_scene.find_child("BlankTilesetIcon"): "BoxMesh",
		self.docked_scene.find_child("OrbitCameraIcon"): "Camera3D",
		self.docked_scene.find_child("ConnectIcon"): "Connect",
		self.docked_scene.find_child("ConnectedIcon"): "StatusSuccess",
		self.token_panel.find_child("ActionIcon"): "Play",
	}
	for control in texture_bindings:
		if control is TextureRect:
			(control as TextureRect).texture = editor_base.get_theme_icon(texture_bindings[control], "EditorIcons")

func on_visit_depot() -> void:
	OS.shell_open("https://ion.cesium.com/assetdepot")

func on_token_panel_pressed() -> void:
	self.token_panel.popup()

func set_session_buttons_enabled(enabled: bool) -> void:
	var utilityButtons := self.get_utility_buttons();
	utilityButtons.append_array(self.ion_asset_buttons)
	for btn in utilityButtons:
		if btn == null: continue
		(btn as Button).disabled = !enabled

func _process(delta: float) -> void:
	if self.ion_session == null:
		return
	self.set_session_buttons_enabled(self.ion_session.is_connected)

# All of the buttons that become available once the user logs in
func get_utility_buttons() -> Array[Button]:
	return [self.add_button, self.upload_button, self.sign_out_button]

func on_upload_pressed() -> void:
	OS.shell_open("https://ion.cesium.com/addasset?")

func on_learn_pressed() -> void:
	CesiumGDPanel.open_learn_page()

func on_help_pressed() -> void:
	CesiumGDPanel.open_help_page()

func toggle_connected(state: bool) -> void:
	self.connected_indicator.visible = state
	self.connect_button.disabled = state
	self.connect_button.visible = !state

func on_connect_pressed() -> void:
	if self.ion_session.is_connecting:
		self.ion_session.cancel_connection()
		self.toggle_connected(false)
		return
	self.ion_session.connect_to_ion()

func on_sign_out_pressed():
	self.ion_session.sign_out()
	self.toggle_connected(false)

func _on_ion_session_state_changed(_state: int) -> void:
	if self.ion_session == null:
		return
	self.toggle_connected(self.ion_session.is_connected)
	self.set_session_buttons_enabled(self.ion_session.is_connected)
	if self.ion_session.is_connected:
		self.add_ion_buttons()

func _on_ion_authorization_failed(message: String) -> void:
	push_error("Cesium ion sign-in failed: " + message)

func on_georef_checked(is_checked: bool) -> void:
	self.cesium_builder_node.use_georeferences = is_checked

func add_tileset():
	self.cesium_builder_node.instantiate_tileset(CesiumAssetBuilder.TILESET_TYPE.Blank, "", "Blank")

func create_dynamic_camera():
	self.cesium_builder_node.instantiate_dynamic_cam()

func create_orbit_camera():
	self.cesium_builder_node.instantiate_orbit_cam()

func fetch_ion_asset_list():
	if self.ion_session == null or not self.ion_session.is_connected:
		return null
	self.ion_session.request_assets()
	var completion: Array = await self.ion_session.operation_completed
	while completion[0] != "assets":
		completion = await self.ion_session.operation_completed
	if not completion[1]:
		push_error(str(completion[3]))
		return null
	var items := completion[2] as Array
	var by_type: Dictionary[String, Array] = {}
	for item in items:
		if not item is Dictionary:
			continue
		var item_type := str((item as Dictionary).get("type", "Unknown"))
		if item_type not in by_type:
			by_type[item_type] = Array()
		by_type[item_type].append(item)
	
	return by_type

func make_button_container(type: String, entries: Array):
	var foldable = BoxContainer.new()
	# foldable.title = type
	var margin = MarginContainer.new()
	margin.set_anchors_preset(Control.PRESET_FULL_RECT)
	margin.offset_top = 10
	margin.offset_bottom = 10
	margin.offset_left = 10
	margin.offset_right = 10
	var assets_list = VBoxContainer.new()
	for entry in entries:
		var hbox = HBoxContainer.new()
		var label = Label.new()
		label.text = str(entry.get("name", "Unnamed asset"))
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		label.size_flags_vertical = Control.SIZE_FILL
		label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
		hbox.add_child(label)
		var button = Button.new()
		button.icon = get_editor_interface().get_base_control().get_theme_icon("Add", "EditorIcons")
		button.icon_alignment = HORIZONTAL_ALIGNMENT_CENTER
		button.set_meta("ion_name", str(entry.get("name", "Unnamed asset")))
		button.pressed.connect(func():
			self.cesium_builder_node.instantiate_tileset(
				int(entry.get("id", 0)),
				type,
				str(entry.get("name", "Unnamed asset")),
			)
		)
		button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		hbox.add_child(button)
		assets_list.add_child(hbox)
	margin.add_child(assets_list)
	foldable.add_child(margin)
	ion_button_holder.add_child(foldable)

func add_ion_buttons() -> void:
	self.ion_button_holder = self.docked_scene.find_child("IonAssetButtonHolder") as Control
	for child in self.ion_button_holder.get_children():
		self.ion_button_holder.remove_child(child)
	var available_assets = await fetch_ion_asset_list()
	self.set_process(true)
	if available_assets == null:
		return
	for type in available_assets.keys():
		make_button_container(type, available_assets[type])
