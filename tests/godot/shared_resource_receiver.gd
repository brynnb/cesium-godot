extends Cesium3DTilesetLifecycleEventReceiver

var textures: Array[Texture2D] = []
var shaders: Array[Shader] = []
var materials: Array[Material] = []
var customized_materials: Array[Material] = []
var meshes: Array[Mesh] = []
var loaded_tiles: Array[Cesium3DTile] = []
var unload_count := 0


func _init() -> void:
	material_selector = Callable(self, "_create_material")
	material_customizing.connect(_customize_material)
	tile_loaded.connect(_on_tile_loaded)
	tile_unloading.connect(_on_tile_unloading)


func _create_material(
	_primitive: CesiumLoadedTilePrimitive,
	default_material: Material
) -> Material:
	var shader_material := default_material as ShaderMaterial
	materials.append(default_material)
	if shader_material != null:
		if shader_material.shader != null:
			shaders.append(shader_material.shader)
		var texture := shader_material.get_shader_parameter(
			"base_color_texture"
		) as Texture2D
		if texture != null:
			textures.append(texture)
	return default_material


func _customize_material(
	primitive: CesiumLoadedTilePrimitive,
	material: Material
) -> void:
	material.set_meta(
		"test_tile_instance_id",
		primitive.loaded_tile.get_instance_id()
	)
	customized_materials.append(material)


func _on_tile_loaded(tile: Cesium3DTile) -> void:
	loaded_tiles.append(tile)
	meshes.append(tile.mesh)


func _on_tile_unloading(_tile: Cesium3DTile) -> void:
	unload_count += 1
