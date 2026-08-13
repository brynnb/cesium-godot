extends Cesium3DTilesetLifecycleEventReceiver

var attachments: Array[Dictionary] = []
var detachments: Array[Dictionary] = []


func _on_raster_overlay_attached(
	binding: CesiumRasterOverlayBinding
) -> void:
	var texture := binding.texture
	var image: Image = texture.get_image() if texture != null else null
	var material := binding.material as ShaderMaterial
	var shader := material.shader if material != null else null
	attachments.append({
		"binding": binding,
		"tile_id": (
			binding.tile_primitive.tile_id
			if binding.tile_primitive != null
			else ""
		),
		"overlay_key": binding.overlay_key,
		"texture_id": texture.get_instance_id() if texture != null else 0,
		"image_width": image.get_width() if image != null else 0,
		"image_height": image.get_height() if image != null else 0,
		"pixel": image.get_pixel(0, 0) if image != null else Color.TRANSPARENT,
		"coordinate_index": binding.texture_coordinate_index,
		"translation": binding.translation,
		"scale": binding.scale,
		"material_valid": material != null,
		"material_default_key": (
			material.get_meta("cesium_default_raster_overlay_key", "")
			if material != null
			else ""
		),
		"shader_code": shader.code if shader != null else "",
		"shader_texture_id": (
			material.get_shader_parameter("overlay0_texture").get_instance_id()
			if (
				material != null and
				material.get_shader_parameter("overlay0_texture") != null
			)
			else 0
		),
		"shader_coordinate_index": (
			int(material.get_shader_parameter(
				"overlay0_texture_coordinate_index"
			))
			if material != null
			else -1
		),
	})


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
