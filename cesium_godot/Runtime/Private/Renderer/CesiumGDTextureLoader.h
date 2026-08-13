/*
 * Godot image-upload adaptation for Cesium Native glTF image assets.
 * Cesium for Unreal counterpart:
 * - Source/CesiumRuntime/Private/CesiumTextureUtility.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0.
 */
#ifndef CESIUM_GD_TEXTURE_LOADER_H
#define CESIUM_GD_TEXTURE_LOADER_H

#include "CesiumImage/ImageAsset.h"
#include "CesiumImage/Ktx2TranscodeTargets.h"
#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "scene/resources/image_texture.h"
#endif


#include "CesiumGltf/Image.h"

class CesiumGDTextureLoader {

public:
	static Ref<ImageTexture> load_image_texture(
		const CesiumImage::ImageAsset& image,
		bool generateMipMapsIfMissing
	);
	static CesiumImage::SupportedGpuCompressedPixelFormats
	get_supported_gpu_compressed_pixel_formats();
private:
	static Error try_get_image_format(
		const CesiumImage::ImageAsset& image,
		Image::Format* outFormat
	);

};

#endif // !CESIUM_GD_TEXTURE_LOADER_H
