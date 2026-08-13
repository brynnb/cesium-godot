@icon("res://addons/cesium_godot/resources/icons/orbit.svg")

class_name CesiumOrbitCamera extends AbstractCesiumCamera

@export var target: CesiumGeoreference

## The minimum distance to the target (meters)
@export var min_distance: float = 3.0
## The maximum distance to the target (meters)
@export var max_distance: float = 40_000_000.0

## Mouse movement sensitivity
@export var pan_sensitivity: float = 1000
## Zoom sensitivity
@export var zoom_sensitivity: float = 10.0

var vel = Vector3(0.0, 0.0, 0.0)
var current_surface_height: float = 6_800_000.0
var command_altitude: float = 35_000_000.0
var altitude: float = 0.0
var dragging: bool = false


func _ready() -> void:
	super()
	if self.globe_node.origin_type == CesiumGeoreference.TrueOrigin:
		self.command_altitude = self.global_position.z


func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		dragging = !dragging and event.button_index == MouseButton.MOUSE_BUTTON_MIDDLE and event.pressed
		if dragging:
			Input.set_default_cursor_shape(Input.CURSOR_DRAG)
		else:
			Input.set_default_cursor_shape(Input.CURSOR_ARROW)
		
		if event.button_index == MouseButton.MOUSE_BUTTON_WHEEL_UP and event.pressed:
			var rect = self.get_viewport().get_visible_rect().size
			var ar = rect.x/rect.y
			self.command_altitude = clampf(self.command_altitude*0.8, self.min_distance, self.max_distance)
			var rel = 2.0*(self.get_viewport().get_mouse_position()/rect-Vector2(0.5, 0.5))
			var v = pan_sensitivity * self.altitude/self.max_distance * rel
			vel.x += 0.05*v.x
			vel.y += 0.05*v.y/ar

		elif event.button_index == MouseButton.MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			self.command_altitude = clampf(self.command_altitude*1.15, self.min_distance, self.max_distance)
	
	if event is InputEventMouseMotion:
		if dragging and event.screen_velocity.length() > 10.0:
			var rect = self.get_viewport().get_visible_rect().size
			var ar = rect.x/rect.y
			var rel = event.screen_relative/rect
			var v = -pan_sensitivity * self.altitude/self.max_distance * rel
			vel.x = v.x
			vel.y = v.y/ar

func _physics_process(delta: float) -> void:
	# Find distance to the ground
	var space = get_world_3d().direct_space_state
	var query = PhysicsRayQueryParameters3D.create(self.global_position, self.globe_node.global_position)
	var result: Dictionary = space.intersect_ray(query)
	if result:
		self.current_surface_height = result.position.length()
		self.altitude = self.global_position.length() - self.current_surface_height
		self.last_hit_distance = self.altitude
		
	var error = self.command_altitude - self.altitude
	vel.z = zoom_sensitivity * delta * error
	
	if dragging or abs(error) > 0.1 * self.min_distance:
		var dx = Transform3D().rotated(self.transform.basis.x, vel.y * delta).rotated(Vector3.UP, vel.x * delta)
		self.transform.basis = dx.basis * self.transform.basis
		self.transform.origin = self.target.global_position \
			+ self.transform.basis.z \
			* clampf(
				self.current_surface_height + self.altitude + vel.z,
				self.min_distance + self.current_surface_height,
				self.max_distance
			)
	
	vel *= Vector3(0.75, 0.75, 1.0)


func _upate_target(new_node: Node3D) -> void:
	target = new_node
