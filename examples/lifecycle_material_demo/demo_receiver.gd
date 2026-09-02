extends Cesium3DTilesetLifecycleEventReceiver

signal event_recorded(message: String)
signal primitive_ready(primitive: CesiumLoadedTilePrimitive)
signal tile_ready(tile: Cesium3DTile)

var hook_counts: Dictionary = {}
var latest_primitive: CesiumLoadedTilePrimitive


func _init() -> void:
	material_selector = Callable(self, "select_material")
	material_customizing.connect(customize_material_parameters)
	tile_mesh_primitive_loaded.connect(record_primitive_loaded)
	raster_overlay_attached.connect(record_raster_attached)
	raster_overlay_detaching.connect(record_raster_detaching)
	tile_loaded.connect(record_tile_loaded)
	tile_visibility_changed.connect(record_visibility_changed)
	tile_unloading.connect(record_tile_unloading)


func _record(event_name: String, detail: String) -> void:
	hook_counts[event_name] = int(hook_counts.get(event_name, 0)) + 1
	var message := "%s  %s" % [event_name, detail]
	print("Cesium demo: ", message)
	event_recorded.emit(message)


func select_material(
	primitive: CesiumLoadedTilePrimitive,
	_default_material: Material
) -> Material:
	_record(
		"material_selector",
		"surface %d: selecting demo shader" % primitive.surface_index
	)
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec4 demo_tint : source_color = vec4(0.12, 0.58, 0.95, 1.0);
uniform sampler2D demo_overlay_texture : source_color, filter_linear_mipmap;
uniform vec4 demo_overlay_translation_scale = vec4(0.0, 0.0, 1.0, 1.0);
uniform int demo_overlay_texture_coordinate_index = -1;
uniform vec3 cesium_absolute_origin_high;
uniform vec3 cesium_absolute_origin_low;
uniform mat3 cesium_local_to_absolute_basis;

void fragment() {
	vec3 color = demo_tint.rgb;
	if (demo_overlay_texture_coordinate_index >= 0) {
		vec2 source_uv = demo_overlay_texture_coordinate_index == 1 ? UV2 : UV;
		vec2 overlay_uv = source_uv * demo_overlay_translation_scale.zw
			+ demo_overlay_translation_scale.xy;
		color = mix(color, texture(demo_overlay_texture, overlay_uv).rgb, 0.72);
	}
	ALBEDO = color;
	ROUGHNESS = 0.82;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material


func customize_material_parameters(
	primitive: CesiumLoadedTilePrimitive,
	material: Material
) -> void:
	var shader_material := material as ShaderMaterial
	if shader_material != null:
		var hue := fmod(0.56 + float(primitive.surface_index) * 0.09, 1.0)
		shader_material.set_shader_parameter(
			"demo_tint",
			Color.from_hsv(hue, 0.75, 0.95, 1.0)
		)
	_record(
		"material_customizing",
		"surface %d: applying per-tile parameters" % primitive.surface_index
	)


func record_primitive_loaded(
	primitive: CesiumLoadedTilePrimitive
) -> void:
	latest_primitive = primitive
	_record(
		"tile_mesh_primitive_loaded",
		"tile=%s surface=%d" % [primitive.tile_id, primitive.surface_index]
	)
	primitive_ready.emit(primitive)


func record_raster_attached(binding: CesiumRasterOverlayBinding) -> void:
	_record(
		"raster_overlay_attached",
		"%s on UV%d" % [binding.overlay_key, binding.texture_coordinate_index]
	)


func record_raster_detaching(binding: CesiumRasterOverlayBinding) -> void:
	_record(
		"raster_overlay_detaching",
		"%s while binding is still valid" % binding.overlay_key
	)


func record_tile_loaded(tile: Cesium3DTile) -> void:
	_record("tile_loaded", "tile=%s" % tile.tile_id)
	tile_ready.emit(tile)


func record_visibility_changed(tile: Cesium3DTile, visible: bool) -> void:
	_record(
		"tile_visibility_changed",
		"tile=%s visible=%s" % [tile.tile_id, visible]
	)


func record_tile_unloading(tile: Cesium3DTile) -> void:
	_record(
		"tile_unloading",
		"tile=%s valid=%s" % [tile.tile_id, is_instance_valid(tile)]
	)
	latest_primitive = null
