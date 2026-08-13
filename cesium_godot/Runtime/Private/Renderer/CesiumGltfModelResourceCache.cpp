// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Private/Renderer/CesiumGltfModelResourceCache.h"

CesiumGltfSharedModelResource::~CesiumGltfSharedModelResource() {
	if (this->statistics == nullptr) {
		return;
	}
	this->statistics->liveSharedModelCount.fetch_sub(
		1,
		std::memory_order_relaxed
	);
	this->statistics->liveSharedModelGeometryBytes.fetch_sub(
		this->geometryBytes,
		std::memory_order_relaxed
	);
	this->statistics->liveSharedModelTextureBytes.fetch_sub(
		this->textureBytes,
		std::memory_order_relaxed
	);
}

CesiumGltfModelResourceCache::CesiumGltfModelResourceCache(
	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics
) : m_statistics(statistics) {}

std::shared_ptr<CesiumGltfSharedModelResource>
CesiumGltfModelResourceCache::acquire(const std::string& contentKey) {
	if (contentKey.empty()) {
		return nullptr;
	}
	this->prune_expired();
	auto existing = this->m_resources.find(contentKey);
	if (existing != this->m_resources.end()) {
		std::shared_ptr<CesiumGltfSharedModelResource> resource =
			existing->second.lock();
		if (resource != nullptr) {
			if (this->m_statistics != nullptr) {
				this->m_statistics->sharedModelCacheHitCount.fetch_add(
					1,
					std::memory_order_relaxed
				);
			}
			return resource;
		}
		this->m_resources.erase(existing);
	}
	if (this->m_statistics != nullptr) {
		this->m_statistics->sharedModelCacheMissCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
	}
	return nullptr;
}

std::shared_ptr<CesiumGltfSharedModelResource>
CesiumGltfModelResourceCache::publish(
	const std::string& contentKey,
	const std::shared_ptr<CesiumGltfSharedModelResource>& resource
) {
	if (contentKey.empty() || resource == nullptr) {
		return resource;
	}
	this->prune_expired();
	auto existing = this->m_resources.find(contentKey);
	if (existing != this->m_resources.end()) {
		std::shared_ptr<CesiumGltfSharedModelResource> live =
			existing->second.lock();
		if (live != nullptr) {
			return live;
		}
		this->m_resources.erase(existing);
	}

	resource->contentKey = contentKey;
	this->m_resources.emplace(contentKey, resource);
	if (this->m_statistics != nullptr) {
		const uint64_t liveCount =
			this->m_statistics->liveSharedModelCount.fetch_add(
				1,
				std::memory_order_relaxed
			) + 1;
		const uint64_t liveGeometry =
			this->m_statistics->liveSharedModelGeometryBytes.fetch_add(
				resource->geometryBytes,
				std::memory_order_relaxed
			) + resource->geometryBytes;
		const uint64_t liveTextures =
			this->m_statistics->liveSharedModelTextureBytes.fetch_add(
				resource->textureBytes,
				std::memory_order_relaxed
			) + resource->textureBytes;
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedModelCount,
			liveCount
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedModelGeometryBytes,
			liveGeometry
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumLiveSharedModelTextureBytes,
			liveTextures
		);
		resource->statistics = this->m_statistics;
	}
	return resource;
}

void CesiumGltfModelResourceCache::prune_expired() {
	for (auto it = this->m_resources.begin(); it != this->m_resources.end();) {
		if (it->second.expired()) {
			it = this->m_resources.erase(it);
		} else {
			++it;
		}
	}
}
