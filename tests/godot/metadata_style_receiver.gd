extends Cesium3DTilesetLifecycleEventReceiver

var loaded_primitives: Array[CesiumLoadedTilePrimitive] = []


func _create_material(
	_primitive: CesiumLoadedTilePrimitive,
	default_material: Material
) -> Material:
	# Exercise the documented lifecycle path: returning the generated material
	# requests a shallow per-tile duplicate while preserving its shader contract.
	return default_material


func _on_tile_mesh_primitive_loaded(
	primitive: CesiumLoadedTilePrimitive
) -> void:
	loaded_primitives.append(primitive)
