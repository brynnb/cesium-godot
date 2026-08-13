// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"

#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"

#include <algorithm>
#include <mutex>

namespace {
struct CesiumGltfImageAssetGodotExtension {
	static inline constexpr const char* TypeName =
		"CesiumGltfImageAssetGodotExtension";
	static inline constexpr const char* ExtensionName =
		"PRIVATE_ImageAsset_Godot";

	std::shared_ptr<CesiumGltfSharedImageResource> resource;
};

std::mutex image_resource_mutex;
std::vector<std::weak_ptr<CesiumGltfSharedImageResource>> all_image_resources;

void fingerprint_bytes(
	CesiumGltfImageContentFingerprint& result,
	const void* source,
	size_t size
) {
	const uint8_t* bytes = static_cast<const uint8_t*>(source);
	for (size_t index = 0; index < size; ++index) {
		result.first ^= static_cast<uint64_t>(bytes[index]);
		result.first *= UINT64_C(1099511628211);
		result.second ^= static_cast<uint64_t>(bytes[index]) +
			UINT64_C(0x9e3779b97f4a7c15) +
			(result.second << 6U) + (result.second >> 2U);
		result.second *= UINT64_C(0xbf58476d1ce4e5b9);
	}
}

template <typename T>
void fingerprint_value(
	CesiumGltfImageContentFingerprint& result,
	const T& value
) {
	fingerprint_bytes(result, &value, sizeof(T));
}

bool has_identical_texture_content(
	const CesiumGltfSharedImageResource& left,
	const CesiumGltfImageContentFingerprint& contentFingerprint,
	const CesiumImage::ImageAsset& right
) {
	if (
		left.contentFingerprint != contentFingerprint ||
		left.width != right.width || left.height != right.height ||
		left.channels != right.channels ||
		left.bytesPerChannel != right.bytesPerChannel ||
		left.compressedPixelFormat != right.compressedPixelFormat ||
		left.mipPositions.size() != right.mipPositions.size()
	) {
		return false;
	}
	for (size_t index = 0; index < left.mipPositions.size(); ++index) {
		if (
			left.mipPositions[index].byteOffset !=
				right.mipPositions[index].byteOffset ||
			left.mipPositions[index].byteSize !=
				right.mipPositions[index].byteSize
		) {
			return false;
		}
	}
	return true;
}

}

size_t CesiumGltfImageContentFingerprintHash::operator()(
	const CesiumGltfImageContentFingerprint& value
) const noexcept {
	return static_cast<size_t>(
		value.first ^
		((value.second << 23U) | (value.second >> 41U)) ^
		(value.pixelBytes * UINT64_C(0x9e3779b97f4a7c15))
	);
}

CesiumGltfImageContentFingerprint
CesiumGltfImageAssetResourceCache::fingerprint(
	const CesiumImage::ImageAsset& imageAsset
) {
	std::scoped_lock lock(image_resource_mutex);
	CesiumGltfImageContentFingerprint result;
	result.first = UINT64_C(1469598103934665603);
	result.second = UINT64_C(1099511628211);
	result.pixelBytes = static_cast<uint64_t>(imageAsset.pixelData.size());
	fingerprint_value(result, imageAsset.width);
	fingerprint_value(result, imageAsset.height);
	fingerprint_value(result, imageAsset.channels);
	fingerprint_value(result, imageAsset.bytesPerChannel);
	const auto compressed = static_cast<int32_t>(imageAsset.compressedPixelFormat);
	fingerprint_value(result, compressed);
	for (const CesiumImage::ImageAssetMipPosition& mip : imageAsset.mipPositions) {
		fingerprint_value(result, mip.byteOffset);
		fingerprint_value(result, mip.byteSize);
	}
	if (!imageAsset.pixelData.empty()) {
		fingerprint_bytes(
			result,
			imageAsset.pixelData.data(),
			imageAsset.pixelData.size()
		);
	}
	return result;
}

CesiumGltfSharedImageResource::~CesiumGltfSharedImageResource() {
	if (this->statistics == nullptr) {
		return;
	}
	this->statistics->liveSharedTextureCount.fetch_sub(
		1,
		std::memory_order_relaxed
	);
	this->statistics->liveSharedTextureBytes.fetch_sub(
		this->sizeBytes,
		std::memory_order_relaxed
	);
}

CesiumGltfImageAssetResourceCache::CesiumGltfImageAssetResourceCache(
	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics
) : m_statistics(statistics) {}

CesiumGltfImageAssetResourceCache::~CesiumGltfImageAssetResourceCache() {
	this->release_generation_resources();
}

void CesiumGltfImageAssetResourceCache::release_all_renderer_resources() {
	std::scoped_lock lock(image_resource_mutex);
	for (const auto& candidate : all_image_resources) {
		const std::shared_ptr<CesiumGltfSharedImageResource> resource =
			candidate.lock();
		if (resource != nullptr) {
			resource->texture.unref();
		}
	}
	all_image_resources.clear();
}

void CesiumGltfImageAssetResourceCache::release_generation_resources() {
	if (this->m_statistics != nullptr && !this->m_shaders.empty()) {
		this->m_statistics->liveSharedShaderCount.fetch_sub(
			static_cast<uint64_t>(this->m_shaders.size()),
			std::memory_order_relaxed
		);
	}
	this->m_shaders.clear();
}

std::shared_ptr<CesiumGltfSharedImageResource>
CesiumGltfImageAssetResourceCache::acquire(
	const CesiumUtility::IntrusivePointer<CesiumImage::ImageAsset>& imageAsset,
	const CesiumGltfImageContentFingerprint& contentFingerprint,
	bool preserveCpuPixelData,
	Error* error
) {
	ERR_FAIL_NULL_V(error, nullptr);
	if (!imageAsset) {
		*error = Error::ERR_INVALID_DATA;
		return nullptr;
	}

	std::scoped_lock lock(image_resource_mutex);
	this->prune_expired();
	auto* extension = imageAsset->getExtension<
		CesiumGltfImageAssetGodotExtension>();
	if (extension != nullptr && extension->resource != nullptr) {
		auto& localResources =
			this->m_contentResources[extension->resource->contentFingerprint];
		const bool alreadyRegistered = std::any_of(
			localResources.begin(),
			localResources.end(),
			[&extension](const auto& candidate) {
				return candidate.lock() == extension->resource;
			}
		);
		if (!alreadyRegistered) {
			localResources.emplace_back(extension->resource);
		}
		if (this->m_statistics != nullptr) {
			this->m_statistics->sharedTextureCacheHitCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
		}
		return extension->resource;
	}
	auto content = this->m_contentResources.find(contentFingerprint);
	if (content != this->m_contentResources.end()) {
		for (auto candidate = content->second.begin();
			 candidate != content->second.end();) {
			std::shared_ptr<CesiumGltfSharedImageResource> resource =
				candidate->lock();
			if (resource == nullptr) {
				candidate = content->second.erase(candidate);
				continue;
			}
			if (has_identical_texture_content(
				*resource,
				contentFingerprint,
				*imageAsset
			)) {
				auto& imageExtension = imageAsset->addExtension<
					CesiumGltfImageAssetGodotExtension>();
				imageExtension.resource = resource;
				const uint64_t releasedBytes = preserveCpuPixelData
					? 0
					: CesiumGDTextureLoader::release_pixel_data(*imageAsset);
				if (this->m_statistics != nullptr) {
					this->m_statistics->sharedTextureCacheHitCount.fetch_add(
						1,
						std::memory_order_relaxed
					);
					if (releasedBytes > 0) {
						this->m_statistics->releasedCpuTextureCount.fetch_add(
							1,
							std::memory_order_relaxed
						);
						this->m_statistics->releasedCpuTextureBytes.fetch_add(
							releasedBytes,
							std::memory_order_relaxed
						);
					}
				}
				return resource;
			}
			++candidate;
		}
		if (content->second.empty()) {
			this->m_contentResources.erase(content);
		}
	}

	Ref<ImageTexture> texture = CesiumGDTextureLoader::load_image_texture(
		*imageAsset,
		true
	);
	if (texture.is_null()) {
		*error = Error::ERR_CANT_CREATE;
		return nullptr;
	}

	auto resource = std::make_shared<CesiumGltfSharedImageResource>();
	resource->texture = texture;
	resource->contentFingerprint = contentFingerprint;
	resource->width = imageAsset->width;
	resource->height = imageAsset->height;
	resource->channels = imageAsset->channels;
	resource->bytesPerChannel = imageAsset->bytesPerChannel;
	resource->compressedPixelFormat = imageAsset->compressedPixelFormat;
	resource->mipPositions = imageAsset->mipPositions;
	resource->sizeBytes = static_cast<uint64_t>(std::max<int64_t>(
		0,
		imageAsset->getSizeBytes()
	));
	auto& imageExtension = imageAsset->addExtension<
		CesiumGltfImageAssetGodotExtension>();
	imageExtension.resource = resource;
	this->m_contentResources[contentFingerprint].emplace_back(resource);
	all_image_resources.emplace_back(resource);
	const uint64_t releasedBytes = preserveCpuPixelData
		? 0
		: CesiumGDTextureLoader::release_pixel_data(*imageAsset);

	if (this->m_statistics != nullptr) {
		this->m_statistics->sharedTextureCacheMissCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		const uint64_t liveCount =
			this->m_statistics->liveSharedTextureCount.fetch_add(
				1,
				std::memory_order_relaxed
			) + 1;
		const uint64_t liveBytes =
			this->m_statistics->liveSharedTextureBytes.fetch_add(
				resource->sizeBytes,
				std::memory_order_relaxed
			) + resource->sizeBytes;
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedTextureCount,
			liveCount
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedTextureBytes,
			liveBytes
		);
		// Install the destructor-side decrement only after every corresponding
		// increment succeeds. This also keeps an allocation failure while adding
		// the weak cache entry from underflowing otherwise untouched counters.
		resource->statistics = this->m_statistics;
		if (releasedBytes > 0) {
			this->m_statistics->releasedCpuTextureCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
			this->m_statistics->releasedCpuTextureBytes.fetch_add(
				releasedBytes,
				std::memory_order_relaxed
			);
		}
	}
	return resource;
}

Ref<Shader> CesiumGltfImageAssetResourceCache::acquire_shader(
	const String& source
) {
	const CharString utf8 = source.utf8();
	const std::string key(utf8.get_data(), static_cast<size_t>(utf8.length()));
	auto existing = this->m_shaders.find(key);
	if (existing != this->m_shaders.end()) {
		if (this->m_statistics != nullptr) {
			this->m_statistics->sharedShaderCacheHitCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
		}
		return existing->second;
	}

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(source);
	this->m_shaders.emplace(key, shader);
	if (this->m_statistics != nullptr) {
		this->m_statistics->sharedShaderCacheMissCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		const uint64_t liveCount =
			this->m_statistics->liveSharedShaderCount.fetch_add(
				1,
				std::memory_order_relaxed
			) + 1;
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedShaderCount,
			liveCount
		);
	}
	return shader;
}

void CesiumGltfImageAssetResourceCache::prune_expired() {
	for (auto content = this->m_contentResources.begin();
		 content != this->m_contentResources.end();) {
		auto& resources = content->second;
		resources.erase(
			std::remove_if(
				resources.begin(),
				resources.end(),
				[](const auto& resource) { return resource.expired(); }
			),
			resources.end()
		);
		if (resources.empty()) {
			content = this->m_contentResources.erase(content);
		} else {
			++content;
		}
	}
}
