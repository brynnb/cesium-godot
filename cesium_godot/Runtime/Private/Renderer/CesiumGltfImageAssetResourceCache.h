// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_GLTF_IMAGE_ASSET_RESOURCE_CACHE_H
#define CESIUM_GLTF_IMAGE_ASSET_RESOURCE_CACHE_H

/**
 * Godot counterpart of Cesium for Unreal v2.29.0's
 * ExtensionImageAssetUnreal and shared FCesiumTextureResource ownership.
 *
 * Cesium Native may expose one ImageAsset through many concurrently loaded
 * glTF models, or independently decode identical embedded image bytes. A
 * worker-computed content fingerprint finds the latter without hashing on the
 * render thread, and exact format/mipmap/pixel comparison prevents a hash
 * collision from aliasing different textures. One cache lease owns one Godot
 * ImageTexture; tiles share the lease, and the resource is released on the
 * main thread when the final tile lease disappears. Generated shader source is
 * also immutable, so one tileset-generation cache retains each exact variant
 * instead of asking Godot to compile it once per tile.
 */

#include "CesiumImage/ImageAsset.h"
#include "CesiumUtility/IntrusivePointer.h"
#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/resources/image_texture.h"
#include "scene/resources/shader.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/image_texture.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/shader.hpp"
using namespace godot;
#endif

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct CesiumGltfImageContentFingerprint {
	uint64_t first = 0;
	uint64_t second = 0;
	uint64_t pixelBytes = 0;

	bool operator==(const CesiumGltfImageContentFingerprint& other) const {
		return
			this->first == other.first &&
			this->second == other.second &&
			this->pixelBytes == other.pixelBytes;
	}
};

struct CesiumGltfImageContentFingerprintHash {
	size_t operator()(
		const CesiumGltfImageContentFingerprint& value
	) const noexcept;
};

struct CesiumGltfSharedImageResource {
	Ref<ImageTexture> texture;
	CesiumGltfImageContentFingerprint contentFingerprint;
	int32_t width = 0;
	int32_t height = 0;
	int32_t channels = 0;
	int32_t bytesPerChannel = 0;
	CesiumImage::GpuCompressedPixelFormat compressedPixelFormat =
		CesiumImage::GpuCompressedPixelFormat::NONE;
	std::vector<CesiumImage::ImageAssetMipPosition> mipPositions;
	uint64_t sizeBytes = 0;
	std::shared_ptr<CesiumTilesetRuntimeStatistics> statistics;

	~CesiumGltfSharedImageResource();
};

class CesiumGltfImageAssetResourceCache {
public:
	explicit CesiumGltfImageAssetResourceCache(
		const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics
	);
	~CesiumGltfImageAssetResourceCache();

	std::shared_ptr<CesiumGltfSharedImageResource> acquire(
		const CesiumUtility::IntrusivePointer<CesiumImage::ImageAsset>& imageAsset,
		const CesiumGltfImageContentFingerprint& contentFingerprint,
		bool preserveCpuPixelData,
		Error* error
	);
	static CesiumGltfImageContentFingerprint fingerprint(
		const CesiumImage::ImageAsset& imageAsset
	);
	// Drop Godot renderer references before RenderingServer shutdown. Native's
	// process-global inactive image depot may outlive the GDExtension scene level.
	static void release_all_renderer_resources();
	Ref<Shader> acquire_shader(const String& source);
	void release_generation_resources();

private:
	void prune_expired();

	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_statistics;
	std::unordered_map<
		CesiumGltfImageContentFingerprint,
		std::vector<std::weak_ptr<CesiumGltfSharedImageResource>>,
		CesiumGltfImageContentFingerprintHash
	> m_contentResources;
	std::unordered_map<std::string, Ref<Shader>> m_shaders;
};

#endif // CESIUM_GLTF_IMAGE_ASSET_RESOURCE_CACHE_H
