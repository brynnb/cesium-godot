
extends Node
class_name AtmosphereManager

@export var display_atmosphere: bool = false
@export var globe: CesiumGeoreference
@export var mesh_atmosphere: MeshInstance3D
@export var sun: DirectionalLight3D
@export var camera: Camera3D
var set_layers: bool = false


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta):
	if display_atmosphere:
		update_settings()

		mesh_atmosphere.material_override = material
		##Value assigned to the atmosphere layer mask to try to make cull to the the physical range decals.
		if !set_layers:
			mesh_atmosphere.set_layer_mask_value(20, true)
			mesh_atmosphere.set_layer_mask_value(19, true)
			mesh_atmosphere.set_layer_mask_value(1, false)
			set_layers = true

		var cam_h = self.camera.last_hit_distance
		mesh_atmosphere.position = Vector3(0, 0, -cam_h)

	else:
		mesh_atmosphere.material_override = null
		set_layers = false

@export var base_material: ShaderMaterial = preload("res://addons/cesium_godot/visuals/mat_atmosphere.tres")
@export var atmosphere_settings: AtmosphereSettings = preload("res://addons/cesium_godot/visuals/scripts/atmosphere_settings.tres")

var material : ShaderMaterial
func _ready() -> void:
	material = base_material.duplicate()
	material.set_shader_parameter("AlphaValue", 0.27)
	print("Atmosphere Manager Ready!")

# Function to calculate the oblate radius value based on camera position and globe parameters.
func get_oblate_radius(globe: CesiumGeoreference) -> float:
	const semi_major_alpha : float = 6378137
	const semi_minor_beta : float = 6356752

	# Calculate the camera's ECEF position based on its coordinates.
	var cam_ecef_loc : Vector3
	if self.globe.origin_type == CesiumGeoreference.CartographicOrigin:
		cam_ecef_loc = Vector3(globe.ecefX, globe.ecefY, globe.ecefZ)
	else:
		cam_ecef_loc = self.globe.get_tx_engine_to_ecef() * self.camera.global_position

	# Calcuate the current vector length based on camera position.
	var camera_x_squared = pow(cam_ecef_loc[0], 2)
	var camera_y_squared = pow(cam_ecef_loc[1], 2)
	var camera_z_squared = pow(cam_ecef_loc[2], 2)
	var camera_oblate : float = ((camera_x_squared + camera_y_squared) / (pow(semi_major_alpha, 2))) + (camera_z_squared / (pow(semi_minor_beta, 2)))

	# Estimate the surface elevation of oblate earth, by scaling camera_oblate vector down to 1
	# https://en.wikipedia.org/wiki/Spheroid#Oblate_spheroids

	var oblate_vector : Vector3 = cam_ecef_loc / pow(camera_oblate, 0.5)
	var oblate_earth : float = pow(pow(oblate_vector[0], 2) + pow(oblate_vector[1], 2) + pow(oblate_vector[2], 2), 0.5)-100
	return oblate_earth

func update_settings():
	var source_viewport := get_viewport()
	const radius : float = 6378137.0
	var oblate_radius: float = get_oblate_radius(self.globe)
	#atmosphere_settings.set_properties(material, radius)
	atmosphere_settings.set_properties(material, oblate_radius)
	atmosphere_settings.atmosphere_scale = 0.1

	# Get the camera's position based on its ECEF coordinates
	var centre : Vector3 = globe.global_position # We used to move this around
	material.set_shader_parameter("Cartographic", self.globe.origin_type == CesiumGeoreference.OriginType.CartographicOrigin)
	material.set_shader_parameter("DistanceToSurface", self.camera.last_hit_distance)
	material.set_shader_parameter("PlanetCentre", centre)
	if self.globe.origin_type == CesiumGeoreference.OriginType.CartographicOrigin:
		var cam_ecef_pos := Vector3(self.globe.ecefX, self.globe.ecefY, self.globe.ecefZ)
		var cam_relative_engine_pos : Vector3 = self.globe.get_tx_ecef_to_engine() * cam_ecef_pos
		material.set_shader_parameter("CameraWorldPos", cam_relative_engine_pos)
	else:
		material.set_shader_parameter("CameraWorldPos", self.camera.global_position)
	#atmosphere_settings.set_properties("OceanRadius", radius)
	material.set_shader_parameter("OceanRadius", oblate_radius)
	material.set_shader_parameter("ScreenWidth", source_viewport.size.x)
	material.set_shader_parameter("ScreenHeight", source_viewport.size.y)

	var dir_to_sun := Vector3.UP
	if sun:
		if self.globe.origin_type == CesiumGeoreference.OriginType.CartographicOrigin:
			dir_to_sun = (sun.global_position - centre).normalized()
		else:
			dir_to_sun = sun.transform * Vector3.FORWARD

	material.set_shader_parameter("DirToSun", dir_to_sun)
