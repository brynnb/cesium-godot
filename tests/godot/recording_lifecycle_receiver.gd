extends Cesium3DTilesetLifecycleEventReceiver

var events: Array[String] = []
var primitive_snapshot: Dictionary = {}
var primitive_snapshots: Array[Dictionary] = []
var unloading_tile_was_valid := false
var overlay_snapshots: Array[Dictionary] = []


func _create_material(
	primitive: CesiumLoadedTilePrimitive,
	default_material: Material
) -> Material:
	events.append("create_material")
	var default_shader_material := default_material as ShaderMaterial
	var default_shader := (
		default_shader_material.shader
		if default_shader_material != null
		else null
	)
	var snapshot: Dictionary = {
		"surface_index": primitive.surface_index,
		"node_index": primitive.node_index,
		"mesh_index": primitive.mesh_index,
		"primitive_index": primitive.primitive_index,
		"material_index": primitive.material_index,
		"primitive_mode": primitive.primitive_mode,
		"attributes": primitive.attributes,
		"gltf_material": primitive.gltf_material,
		"tile_id": primitive.tile_id,
		"tile_extras": primitive.tile_extras,
		"tile_valid": primitive.loaded_tile != null,
		"loaded_tile": primitive.loaded_tile,
		"overlay_uv_index": primitive.get_overlay_texture_coordinate_index(0),
		"default_material_is_shader": default_shader_material != null,
		"default_shader_code": default_shader.code if default_shader != null else "",
		"default_base_color_factor": (
			default_shader_material.get_shader_parameter("base_color_factor")
			if default_shader_material != null else null
		),
		"default_metallic_factor": (
			default_shader_material.get_shader_parameter("metallic_factor")
			if default_shader_material != null else null
		),
		"default_roughness_factor": (
			default_shader_material.get_shader_parameter("roughness_factor")
			if default_shader_material != null else null
		),
		"default_emissive_factor": (
			default_shader_material.get_shader_parameter("emissive_factor")
			if default_shader_material != null else null
		),
		"default_normal_scale": (
			default_shader_material.get_shader_parameter("normal_scale")
			if default_shader_material != null else null
		),
		"default_occlusion_strength": (
			default_shader_material.get_shader_parameter("occlusion_strength")
			if default_shader_material != null else null
		),
		"default_alpha_cutoff": (
			default_shader_material.get_shader_parameter("alpha_cutoff")
			if default_shader_material != null else null
		),
		"default_base_texture": (
			default_shader_material.get_shader_parameter("base_color_texture")
			if default_shader_material != null else null
		),
		"default_metallic_roughness_texture": (
			default_shader_material.get_shader_parameter("metallic_roughness_texture")
			if default_shader_material != null else null
		),
		"default_normal_texture": (
			default_shader_material.get_shader_parameter("normal_texture")
			if default_shader_material != null else null
		),
		"default_occlusion_texture": (
			default_shader_material.get_shader_parameter("occlusion_texture")
			if default_shader_material != null else null
		),
		"default_emissive_texture": (
			default_shader_material.get_shader_parameter("emissive_texture")
			if default_shader_material != null else null
		),
		"default_base_uv_set": (
			default_shader_material.get_shader_parameter("base_color_uv_set")
			if default_shader_material != null else null
		),
		"default_base_transform": (
			default_shader_material.get_shader_parameter("base_color_texture_transform")
			if default_shader_material != null else null
		),
		"default_base_rotation": (
			default_shader_material.get_shader_parameter("base_color_texture_rotation")
			if default_shader_material != null else null
		),
		"default_base_wrap": (
			default_shader_material.get_shader_parameter("base_color_texture_wrap")
			if default_shader_material != null else null
		),
		"default_sampler_exact": (
			default_shader_material.get_meta("cesium_sampler_exact", true)
			if default_shader_material != null else null
		),
		"default_material_id": default_material.get_instance_id(),
	}
	primitive_snapshots.append(snapshot)
	if primitive.surface_index == 0:
		primitive_snapshot = snapshot
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
uniform sampler2D overlaya_texture : source_color;
uniform vec4 overlaya_translation_scale;
uniform int overlaya_texture_coordinate_index;
uniform sampler2D overlayb_texture : source_color;
uniform vec4 overlayb_translation_scale;
uniform int overlayb_texture_coordinate_index;
uniform vec3 cesium_absolute_origin_high;
uniform vec3 cesium_absolute_origin_low;
uniform mat3 cesium_local_to_absolute_basis;
void fragment() {
	ALBEDO = vec3(0.1, 0.6, 0.9);
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material


func _customize_material(
	primitive: CesiumLoadedTilePrimitive,
	material: Material
) -> void:
	var shader_material := material as ShaderMaterial
	var snapshot: Dictionary = primitive_snapshots[primitive.surface_index]
	snapshot["absolute_origin"] = primitive.absolute_origin
	snapshot["absolute_origin_high"] = primitive.absolute_origin_high
	snapshot["absolute_origin_low"] = primitive.absolute_origin_low
	snapshot["local_to_absolute_basis"] = primitive.local_to_absolute_basis
	snapshot["shader_origin_high"] = shader_material.get_shader_parameter(
		"cesium_absolute_origin_high"
	)
	snapshot["shader_origin_low"] = shader_material.get_shader_parameter(
		"cesium_absolute_origin_low"
	)
	snapshot["shader_basis"] = shader_material.get_shader_parameter(
		"cesium_local_to_absolute_basis"
	)
	events.append("customize_material")


func _on_tile_mesh_primitive_loaded(
	_primitive: CesiumLoadedTilePrimitive
) -> void:
	events.append("primitive_loaded")


func _on_raster_overlay_attached(binding: CesiumRasterOverlayBinding) -> void:
	var shader_material := binding.material as ShaderMaterial
	overlay_snapshots.append({
		"event": "attached",
		"key": binding.overlay_key,
		"coordinate_id": binding.texture_coordinate_id,
		"coordinate_index": binding.texture_coordinate_index,
		"translation": binding.translation,
		"scale": binding.scale,
		"texture_valid": binding.texture != null,
		"material_valid": binding.material != null,
		"primitive_valid": binding.tile_primitive != null,
		"attached": binding.attached,
		"shader_texture_valid": (
			shader_material != null and
			shader_material.get_shader_parameter(
				binding.shader_parameter_prefix + "_texture"
			) != null
		),
		"overlay_a_still_bound": (
			shader_material != null and
			shader_material.get_shader_parameter("overlaya_texture") != null
		),
	})
	events.append("raster_attached:%s" % binding.overlay_key)


func _on_raster_overlay_detaching(binding: CesiumRasterOverlayBinding) -> void:
	overlay_snapshots.append({
		"event": "detaching",
		"key": binding.overlay_key,
		"attached": binding.attached,
		"material_valid": binding.material != null,
	})
	events.append("raster_detaching:%s" % binding.overlay_key)


func _on_tile_loaded(_tile: Cesium3DTile) -> void:
	events.append("tile_loaded")


func _on_tile_visibility_changed(
	_tile: Cesium3DTile,
	visible: bool
) -> void:
	events.append("visibility:%s" % visible)


func _on_tile_unloading(tile: Cesium3DTile) -> void:
	unloading_tile_was_valid = is_instance_valid(tile)
	events.append("tile_unloading")
