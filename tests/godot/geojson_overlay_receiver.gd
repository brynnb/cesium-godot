extends Cesium3DTilesetLifecycleEventReceiver

var events: Array[String] = []
var attachments: Array[Dictionary] = []
var detachments: Array[Dictionary] = []


func _on_tile_visibility_changed(_tile: Cesium3DTile, visible: bool) -> void:
	events.append("visibility:%s" % visible)


func _on_raster_overlay_attached(
	binding: CesiumRasterOverlayBinding
) -> void:
	var texture := binding.texture
	var image: Image = texture.get_image() if texture != null else null
	var width := image.get_width() if image != null else 0
	var height := image.get_height() if image != null else 0
	var material := binding.material as ShaderMaterial
	var shader_texture: Texture2D = null
	if material != null:
		shader_texture = material.get_shader_parameter(
			binding.shader_parameter_prefix + "_texture"
		) as Texture2D
	attachments.append({
		"binding": binding,
		"tile_id": (
			binding.tile_primitive.tile_id
			if binding.tile_primitive != null
			else ""
		),
		"overlay_key": binding.overlay_key,
		"texture_id": texture.get_instance_id() if texture != null else 0,
		"image_width": width,
		"image_height": height,
		"center_pixel": (
			image.get_pixel(width / 2, height / 2)
			if image != null and width > 0 and height > 0
			else Color.TRANSPARENT
		),
		"quarter_pixel": (
			image.get_pixel(width / 4, height / 4)
			if image != null and width > 0 and height > 0
			else Color.TRANSPARENT
		),
		"coordinate_index": binding.texture_coordinate_index,
		"material_valid": material != null,
		"shader_texture_id": (
			shader_texture.get_instance_id()
			if shader_texture != null
			else 0
		),
	})
	events.append("raster_attached:%s" % binding.overlay_key)


func _on_raster_overlay_detaching(
	binding: CesiumRasterOverlayBinding
) -> void:
	detachments.append({
		"binding": binding,
		"overlay_key": binding.overlay_key,
		"attached_during_callback": binding.attached,
		"texture_valid_during_callback": binding.texture != null,
		"material_valid_during_callback": binding.material != null,
	})
	events.append("raster_detaching:%s" % binding.overlay_key)
