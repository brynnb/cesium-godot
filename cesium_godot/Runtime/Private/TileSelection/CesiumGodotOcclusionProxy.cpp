#include "Runtime/Private/TileSelection/CesiumGodotOcclusionProxy.h"

#include <algorithm>

CesiumGodotOcclusionProxy::CesiumGodotOcclusionProxy(
	CesiumGodotOcclusionProxyPool* pool
) : m_pool(pool) {}

Cesium3DTilesSelection::TileOcclusionState
CesiumGodotOcclusionProxy::getOcclusionState() const {
	return this->m_pool == nullptr
		? Cesium3DTilesSelection::TileOcclusionState::OcclusionUnavailable
		: this->m_pool->get_state(this->get_tile());
}

const Cesium3DTilesSelection::Tile* CesiumGodotOcclusionProxy::get_tile() const {
	return this->m_tile.load(std::memory_order_acquire);
}

void CesiumGodotOcclusionProxy::reset(
	const Cesium3DTilesSelection::Tile* tile
) {
	this->m_tile.store(tile, std::memory_order_release);
}

CesiumGodotOcclusionProxyPool::CesiumGodotOcclusionProxyPool(
	int32_t maximumPoolSize
) : TileOcclusionRendererProxyPool(maximumPoolSize) {}

CesiumGodotOcclusionProxyPool::~CesiumGodotOcclusionProxyPool() {
	this->destroyPool();
}

std::vector<CesiumGodotOcclusionProxy*>
CesiumGodotOcclusionProxyPool::snapshot_active() const {
	std::lock_guard<std::mutex> lock(this->m_mutex);
	std::vector<CesiumGodotOcclusionProxy*> result;
	result.reserve(this->m_proxies.size());
	for (CesiumGodotOcclusionProxy* proxy : this->m_proxies) {
		if (proxy->get_tile() != nullptr) {
			result.push_back(proxy);
		}
	}
	return result;
}

Cesium3DTilesSelection::TileOcclusionState
CesiumGodotOcclusionProxyPool::get_state(
	const Cesium3DTilesSelection::Tile* tile
) const {
	if (tile == nullptr) {
		return Cesium3DTilesSelection::TileOcclusionState::NotOccluded;
	}
	std::lock_guard<std::mutex> lock(this->m_mutex);
	const auto found = this->m_tileStates.find(tile);
	if (
		found == this->m_tileStates.end() ||
		this->m_latestGeneration > found->second.generation + 2
	) {
		return Cesium3DTilesSelection::TileOcclusionState::OcclusionUnavailable;
	}
	return found->second.state;
}

void CesiumGodotOcclusionProxyPool::apply_result(
	const Cesium3DTilesSelection::Tile* tile,
	Cesium3DTilesSelection::TileOcclusionState state,
	uint64_t generation
) {
	if (tile == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lock(this->m_mutex);
	const bool newGeneration = generation > this->m_latestGeneration;
	this->m_latestGeneration = std::max(this->m_latestGeneration, generation);
	this->m_tileStates[tile] = CachedState{state, generation};
	if (!newGeneration) {
		return;
	}
	for (auto iterator = this->m_tileStates.begin(); iterator != this->m_tileStates.end();) {
		if (this->m_latestGeneration > iterator->second.generation + 2) {
			iterator = this->m_tileStates.erase(iterator);
		} else {
			++iterator;
		}
	}
}

Cesium3DTilesSelection::TileOcclusionRendererProxy*
CesiumGodotOcclusionProxyPool::createProxy() {
	auto* proxy = new CesiumGodotOcclusionProxy(this);
	std::lock_guard<std::mutex> lock(this->m_mutex);
	this->m_proxies.push_back(proxy);
	return proxy;
}

void CesiumGodotOcclusionProxyPool::destroyProxy(
	Cesium3DTilesSelection::TileOcclusionRendererProxy* proxy
) {
	auto* godotProxy = static_cast<CesiumGodotOcclusionProxy*>(proxy);
	{
		std::lock_guard<std::mutex> lock(this->m_mutex);
		const auto found = std::find(
			this->m_proxies.begin(),
			this->m_proxies.end(),
			godotProxy
		);
		if (found != this->m_proxies.end()) {
			this->m_proxies.erase(found);
		}
	}
	delete godotProxy;
}
