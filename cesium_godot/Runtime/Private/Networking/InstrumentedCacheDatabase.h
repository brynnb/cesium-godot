#ifndef CESIUM_GODOT_INSTRUMENTED_CACHE_DATABASE_H
#define CESIUM_GODOT_INSTRUMENTED_CACHE_DATABASE_H

#include "CesiumAsync/ICacheDatabase.h"
#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"

#include <memory>

class CesiumLoadFailureQueue;

/**
 * Telemetry wrapper around Cesium Native's SQLite cache database.
 *
 * Cesium for Unreal counterpart:
 * - Source/CesiumRuntime/Private/UnrealAssetAccessor.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0. The wrapper is a
 * Godot-specific adaptation because upstream does not expose this exact
 * statistics boundary.
 */
class InstrumentedCacheDatabase final : public CesiumAsync::ICacheDatabase {
public:
	InstrumentedCacheDatabase(
		const std::shared_ptr<CesiumAsync::ICacheDatabase>& database,
		const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
		const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue,
		uint64_t sourceInstanceId
	);

	std::optional<CesiumAsync::CacheItem> getEntry(
		const std::string& key
	) const override;

	bool storeEntry(
		const std::string& key,
		std::time_t expiryTime,
		const std::string& url,
		const std::string& requestMethod,
		const CesiumAsync::HttpHeaders& requestHeaders,
		uint16_t statusCode,
		const CesiumAsync::HttpHeaders& responseHeaders,
		const std::span<const std::byte>& responseData
	) override;

	bool prune() override;
	bool clearAll() override;

private:
	std::shared_ptr<CesiumAsync::ICacheDatabase> m_database;
	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_statistics;
	std::shared_ptr<CesiumLoadFailureQueue> m_failureQueue;
	uint64_t m_sourceInstanceId = 0;
};

#endif // CESIUM_GODOT_INSTRUMENTED_CACHE_DATABASE_H
