#pragma once

#include <Cesium3DTilesSelection/TileOcclusionRendererProxy.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

class CesiumGodotOcclusionProxyPool;

class CesiumGodotOcclusionProxy final
    : public Cesium3DTilesSelection::TileOcclusionRendererProxy {
public:
	explicit CesiumGodotOcclusionProxy(CesiumGodotOcclusionProxyPool* pool);
	Cesium3DTilesSelection::TileOcclusionState getOcclusionState() const override;
	const Cesium3DTilesSelection::Tile* get_tile() const;

protected:
	void reset(const Cesium3DTilesSelection::Tile* tile) override;

private:
	CesiumGodotOcclusionProxyPool* m_pool = nullptr;
	std::atomic<const Cesium3DTilesSelection::Tile*> m_tile{nullptr};
};

class CesiumGodotOcclusionProxyPool final
    : public Cesium3DTilesSelection::TileOcclusionRendererProxyPool {
public:
	explicit CesiumGodotOcclusionProxyPool(int32_t maximumPoolSize);
	~CesiumGodotOcclusionProxyPool() override;

	std::vector<CesiumGodotOcclusionProxy*> snapshot_active() const;
	Cesium3DTilesSelection::TileOcclusionState get_state(
		const Cesium3DTilesSelection::Tile* tile
	) const;
	void apply_result(
		const Cesium3DTilesSelection::Tile* tile,
		Cesium3DTilesSelection::TileOcclusionState state,
		uint64_t generation
	);

protected:
	Cesium3DTilesSelection::TileOcclusionRendererProxy* createProxy() override;
	void destroyProxy(
		Cesium3DTilesSelection::TileOcclusionRendererProxy* proxy
	) override;

private:
	mutable std::mutex m_mutex;
	std::vector<CesiumGodotOcclusionProxy*> m_proxies;
	struct CachedState {
		Cesium3DTilesSelection::TileOcclusionState state;
		uint64_t generation;
	};
	std::unordered_map<const Cesium3DTilesSelection::Tile*, CachedState>
		m_tileStates;
	uint64_t m_latestGeneration = 0;
};
