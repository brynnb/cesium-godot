#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"
#include "CesiumImage/ImageAsset.h"
#include "CesiumImage/Ktx2TranscodeTargets.h"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "error_names.hpp"

#include <optional>

namespace {
struct GodotTextureCompressionFeatures final {
	bool s3tc = false;
	bool rgtc = false;
	bool bptc = false;
	bool etc1 = false;
	bool etc2 = false;
	bool astc = false;
};

constexpr CesiumImage::SupportedGpuCompressedPixelFormats
supported_formats_for_features(
	const GodotTextureCompressionFeatures& features
) {
	CesiumImage::SupportedGpuCompressedPixelFormats result;
	result.ETC1_RGB = features.etc1 || features.etc2;
	result.ETC2_RGBA = features.etc2;
	result.BC1_RGB = features.s3tc;
	result.BC3_RGBA = features.s3tc;
	// Godot reports BC4/BC5 through RGTC independently of BC1/BC3 S3TC.
	result.BC4_R = features.rgtc;
	result.BC5_RG = features.rgtc;
	result.BC7_RGBA = features.bptc;
	result.ASTC_4x4_RGBA = features.astc;
	result.ETC2_EAC_R11 = features.etc2;
	result.ETC2_EAC_RG11 = features.etc2;
	return result;
}

constexpr std::optional<Image::Format> godot_compressed_image_format(
	CesiumImage::GpuCompressedPixelFormat format
) {
	switch (format) {
	case CesiumImage::GpuCompressedPixelFormat::ETC1_RGB:
		return Image::FORMAT_ETC;
	case CesiumImage::GpuCompressedPixelFormat::ETC2_RGBA:
		return Image::FORMAT_ETC2_RGBA8;
	case CesiumImage::GpuCompressedPixelFormat::BC1_RGB:
		return Image::FORMAT_DXT1;
	case CesiumImage::GpuCompressedPixelFormat::BC3_RGBA:
		return Image::FORMAT_DXT5;
	case CesiumImage::GpuCompressedPixelFormat::BC4_R:
		return Image::FORMAT_RGTC_R;
	case CesiumImage::GpuCompressedPixelFormat::BC5_RG:
		return Image::FORMAT_RGTC_RG;
	case CesiumImage::GpuCompressedPixelFormat::BC7_RGBA:
		return Image::FORMAT_BPTC_RGBA;
	case CesiumImage::GpuCompressedPixelFormat::ASTC_4x4_RGBA:
		return Image::FORMAT_ASTC_4x4;
	case CesiumImage::GpuCompressedPixelFormat::ETC2_EAC_R11:
		return Image::FORMAT_ETC2_R11;
	case CesiumImage::GpuCompressedPixelFormat::ETC2_EAC_RG11:
		return Image::FORMAT_ETC2_RG11;
	case CesiumImage::GpuCompressedPixelFormat::NONE:
	case CesiumImage::GpuCompressedPixelFormat::PVRTC1_4_RGB:
	case CesiumImage::GpuCompressedPixelFormat::PVRTC1_4_RGBA:
	case CesiumImage::GpuCompressedPixelFormat::PVRTC2_4_RGB:
	case CesiumImage::GpuCompressedPixelFormat::PVRTC2_4_RGBA:
		return std::nullopt;
	}
	return std::nullopt;
}

constexpr GodotTextureCompressionFeatures S3TC_ONLY{
	true, false, false, false, false, false
};
constexpr GodotTextureCompressionFeatures RGTC_ONLY{
	false, true, false, false, false, false
};
constexpr GodotTextureCompressionFeatures ETC2_ONLY{
	false, false, false, false, true, false
};
constexpr GodotTextureCompressionFeatures ALL_FEATURES{
	true, true, true, true, true, true
};
constexpr GodotTextureCompressionFeatures NO_FEATURES{};

static_assert(supported_formats_for_features(S3TC_ONLY).BC1_RGB);
static_assert(supported_formats_for_features(S3TC_ONLY).BC3_RGBA);
static_assert(!supported_formats_for_features(S3TC_ONLY).BC4_R);
static_assert(!supported_formats_for_features(S3TC_ONLY).BC5_RG);
static_assert(supported_formats_for_features(RGTC_ONLY).BC4_R);
static_assert(supported_formats_for_features(RGTC_ONLY).BC5_RG);
static_assert(!supported_formats_for_features(RGTC_ONLY).BC1_RGB);
static_assert(supported_formats_for_features(ETC2_ONLY).ETC1_RGB);
static_assert(supported_formats_for_features(ETC2_ONLY).ETC2_RGBA);
static_assert(supported_formats_for_features(ETC2_ONLY).ETC2_EAC_R11);
static_assert(supported_formats_for_features(ETC2_ONLY).ETC2_EAC_RG11);
static_assert(supported_formats_for_features(ALL_FEATURES).BC7_RGBA);
static_assert(supported_formats_for_features(ALL_FEATURES).ASTC_4x4_RGBA);
static_assert(!supported_formats_for_features(NO_FEATURES).BC1_RGB);
static_assert(!supported_formats_for_features(NO_FEATURES).ETC1_RGB);
static_assert(
	godot_compressed_image_format(
		CesiumImage::GpuCompressedPixelFormat::BC5_RG
	) == Image::FORMAT_RGTC_RG
);
static_assert(
	!godot_compressed_image_format(
		CesiumImage::GpuCompressedPixelFormat::PVRTC1_4_RGBA
	).has_value()
);
} // namespace

Ref<ImageTexture> CesiumGDTextureLoader::load_image_texture(
	const CesiumImage::ImageAsset& image,
	bool generateMipMapsIfMissing
)
{
	ERR_FAIL_COND_V_MSG(
		image.width <= 0 || image.height <= 0,
		Ref<ImageTexture>(),
		"Cesium image dimensions must be positive"
	);

	PackedByteArray rawImageData;
	rawImageData.resize(static_cast<int64_t>(image.pixelData.size()));
	uint8_t* destination = rawImageData.ptrw();
	for (size_t i = 0; i < image.pixelData.size(); ++i) {
		destination[i] = std::to_integer<uint8_t>(image.pixelData[i]);
	}

	Image::Format cesiumFormat;
	Error err = try_get_image_format(image, &cesiumFormat);
	ERR_FAIL_COND_V_MSG(err != Error::OK, Ref<ImageTexture>(), "Image format not recognized!");

	const bool sourceHasMipMaps = image.mipPositions.size() > 1;
	Ref<Image> godotImage = Image::create_from_data(
		image.width,
		image.height,
		sourceHasMipMaps,
		cesiumFormat,
		rawImageData
	);
	ERR_FAIL_COND_V_MSG(
		godotImage.is_null() || godotImage->is_empty(),
		Ref<ImageTexture>(),
		"Godot rejected the decoded Cesium image payload"
	);

	// An empty mip-position array means the source requests runtime mipmap
	// generation. Exactly one entry explicitly means a single-level image.
	if (generateMipMapsIfMissing && image.mipPositions.empty()) {
		if (godotImage->is_compressed()) {
			err = godotImage->decompress();
			if (err != Error::OK) {
				WARN_PRINT(
					"A GPU-compressed KTX2 texture requested runtime mipmaps, but "
					"Godot could not decompress it; uploading the base level only"
				);
			}
		}

		if (!godotImage->is_compressed()) {
		err = godotImage->generate_mipmaps();
			if (err != Error::OK) {
				ERR_PRINT(
					String("Mipmaps were not generated! Error: ") +
					REFLECT_ERR_NAME(err)
				);
			}
		}
	}
	Ref<ImageTexture> texture = ImageTexture::create_from_image(godotImage);
	if (texture.is_valid()) {
		texture->set_meta(
			"cesium_gpu_compressed",
			image.compressedPixelFormat !=
				CesiumImage::GpuCompressedPixelFormat::NONE &&
			godotImage->is_compressed()
		);
		texture->set_meta(
			"cesium_compressed_pixel_format",
			static_cast<int32_t>(image.compressedPixelFormat)
		);
		texture->set_meta(
			"cesium_source_byte_size",
			static_cast<int64_t>(image.pixelData.size())
		);
	}
	return texture;
}

uint64_t CesiumGDTextureLoader::release_pixel_data(
	CesiumImage::ImageAsset& image
) {
	const uint64_t releasedBytes =
		static_cast<uint64_t>(image.pixelData.size());
	if (releasedBytes == 0) {
		return 0;
	}
	image.sizeBytes = static_cast<int64_t>(releasedBytes);
	std::vector<std::byte>().swap(image.pixelData);
	std::vector<CesiumImage::ImageAssetMipPosition>().swap(image.mipPositions);
	return releasedBytes;
}

CesiumImage::SupportedGpuCompressedPixelFormats
CesiumGDTextureLoader::get_supported_gpu_compressed_pixel_formats()
{
	CesiumImage::SupportedGpuCompressedPixelFormats result;
	RenderingServer* renderingServer = RenderingServer::get_singleton();
	if (renderingServer == nullptr) {
		return result;
	}

	const bool s3tc = renderingServer->has_os_feature("s3tc");
	const bool rgtc = renderingServer->has_os_feature("rgtc");
	const bool bptc = renderingServer->has_os_feature("bptc");
	const bool etc1 = renderingServer->has_os_feature("etc");
	const bool etc2 = renderingServer->has_os_feature("etc2");
	const bool astc = renderingServer->has_os_feature("astc");
	return supported_formats_for_features(
		GodotTextureCompressionFeatures{s3tc, rgtc, bptc, etc1, etc2, astc}
	);
}

Error CesiumGDTextureLoader::try_get_image_format(
	const CesiumImage::ImageAsset& image,
	Image::Format* outFormat
)
{
	ERR_FAIL_COND_V_MSG(
		outFormat == nullptr,
		Error::ERR_INVALID_PARAMETER,
		"Image format output pointer must be valid"
	);

	if (
		image.compressedPixelFormat !=
		CesiumImage::GpuCompressedPixelFormat::NONE
	) {
		const std::optional<Image::Format> compressedFormat =
			godot_compressed_image_format(image.compressedPixelFormat);
		if (!compressedFormat.has_value()) {
			return Error::ERR_UNAVAILABLE;
		}
		*outFormat = *compressedFormat;
		return Error::OK;
	}

	ERR_FAIL_COND_V_MSG(
		image.channels < 1 || image.channels > 4,
		Error::ERR_FILE_CORRUPT,
		"Cesium image must contain one to four channels"
	);

	constexpr int32_t SINGLE_BYTE_PER_CHANNEL = 1;
	constexpr int32_t FLOATING_POINT_BYTES_PER_CHANNEL = 4;

	switch (image.bytesPerChannel) {
	case SINGLE_BYTE_PER_CHANNEL:
		switch (image.channels) {
		case 1: *outFormat = Image::FORMAT_R8; break;
		case 2: *outFormat = Image::FORMAT_RG8; break;
		case 3: *outFormat = Image::FORMAT_RGB8; break;
		case 4: *outFormat = Image::FORMAT_RGBA8; break;
		default: return Error::ERR_FILE_CORRUPT;
		}
		break;
	case FLOATING_POINT_BYTES_PER_CHANNEL:
		switch (image.channels) {
		case 1: *outFormat = Image::FORMAT_RF; break;
		case 2: *outFormat = Image::FORMAT_RGF; break;
		case 3: *outFormat = Image::FORMAT_RGBF; break;
		case 4: *outFormat = Image::FORMAT_RGBAF; break;
		default: return Error::ERR_FILE_CORRUPT;
		}
		break;
	default:
		return Error::ERR_FILE_CORRUPT;
	}
	return Error::OK;
}
