extends EditorInspectorPlugin

class_name CesiumTooltips


func _can_handle(object: Object) -> bool:
	return object is Cesium3DTileset

func _parse_property(object, type, name, hint_type, hint_string, usage, wide):
	if parse_numeric_properties(object, type, name, hint_type, hint_string, usage, wide):
		return true
	if parse_boolean_properties(object, type, name, hint_string):
		return true
	return false

func parse_boolean_properties(object, type, name, hint_string) -> bool:
	if (type != TYPE_BOOL):
		return false

	var prop = BoolEditorWithTooltip.new()
	prop.setup(object, name, hint_string)
	prop.tooltip_text = hint_string
	prop.editor_description = hint_string
	add_custom_control(prop)
	return true


func parse_numeric_properties(object, type, name, hint_type, hint_string, usage, wide):
	if !object.is_class("Cesium3DTileset"):
		return false
	var accepted_names := ["maximum_screen_space_error", "maximum_simultaneous_tile_loads", "loading_descendant_limit"]
	if (name in accepted_names):
		var prop = NumberEditorWithTooltip.new()
		prop.setup(object, name, hint_string)
		prop.tooltip_text = hint_string
		prop.editor_description = hint_string
		add_custom_control(prop)
		return true
	
	return false

func _add_tooltiped_prop() -> EditorProperty:
	return EditorProperty.new()



# Yeah, we have to do a lot of manual work for this to work... yay Godot!

class NumberEditorWithTooltip extends EditorProperty:
	var spin_box = SpinBox.new()
	var property_name: String
	var edited_object: Object
	var hbox := HBoxContainer.new()

	func _init():
		# Configure layout
		hbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		self.add_child(hbox)
		hbox.add_child(spin_box)
		
		self.spin_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		self.spin_box.max_value = 10000;
		
		spin_box.connect("value_changed", Callable(self, "_on_value_changed"))
		#set_bottom_editor(hbox)

	func setup(object: Object, name: String, hint: String):
		property_name = name
		self.label = name.capitalize()
		edited_object = object
		spin_box.tooltip_text = hint
		spin_box.value = object.get(name)
	
	func _update_property():
		# Update spinbox when property changes externally
		spin_box.value = edited_object.get(property_name)

	func _on_value_changed(value: float):
		if edited_object.get(property_name) != value:
			emit_changed(property_name, value)
			edited_object.set(property_name, value)


# We will eventually make this a derived class of a common property with an HBox
class BoolEditorWithTooltip extends EditorProperty:
	var hbox = HBoxContainer.new()
	var checkbox = CheckBox.new()
	var property_name: String
	var edited_object: Object
	
	func _init():
		hbox.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		add_child(hbox)
		
		# Add label and checkbox
		hbox.add_child(checkbox)
		
		# Configure checkbox
		checkbox.connect("toggled", Callable(self, "_on_toggled"))
		checkbox.size_flags_horizontal = Control.SIZE_SHRINK_END
	
	func setup(object: Object, name: String, hint: String):
		property_name = name
		edited_object = object
		label = name.capitalize()
		hbox.tooltip_text = hint
		checkbox.button_pressed = object.get(name)
	
	func _update_property():
		checkbox.button_pressed = edited_object.get(property_name)
	
	func _on_toggled(toggled: bool):
		if edited_object.get(property_name) != toggled:
			emit_changed(property_name, toggled)
			edited_object.set(property_name, toggled)
