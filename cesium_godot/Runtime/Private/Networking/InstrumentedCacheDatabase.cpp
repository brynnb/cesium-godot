#include "Runtime/Private/Networking/InstrumentedCacheDatabase.h"
#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"
#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"

#include <atomic>
#include <chrono>

InstrumentedCacheDatabase::InstrumentedCacheDatabase(
	const std::shared_ptr<CesiumAsync::ICacheDatabase>& database,
	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
	const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue,
	uint64_t sourceInstanceId
) : m_database(database),
	m_statistics(statistics),
	m_failureQueue(failureQueue),
	m_sourceInstanceId(sourceInstanceId) {
}

std::optional<CesiumAsync::CacheItem> InstrumentedCacheDatabase::getEntry(
	const std::string& key
) const {
	std::optional<CesiumAsync::CacheItem> entry = this->m_database->getEntry(key);
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestCacheLookupCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		(entry.has_value()
			? this->m_statistics->requestCacheHitCount
			: this->m_statistics->requestCacheMissCount)
			.fetch_add(1, std::memory_order_relaxed);
	}
	return entry;
}

bool InstrumentedCacheDatabase::storeEntry(
	const std::string& key,
	std::time_t expiryTime,
	const std::string& url,
	const std::string& requestMethod,
	const CesiumAsync::HttpHeaders& requestHeaders,
	uint16_t statusCode,
	const CesiumAsync::HttpHeaders& responseHeaders,
	const std::span<const std::byte>& responseData
) {
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestCacheStoreAttemptCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
	}
	const bool stored = this->m_database->storeEntry(
		key,
		expiryTime,
		url,
		requestMethod,
		requestHeaders,
		statusCode,
		responseHeaders,
		responseData
	);
	if (stored && this->m_statistics != nullptr) {
		this->m_statistics->requestCacheStoreSuccessCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		this->m_statistics->requestCacheStoredPayloadBytes.fetch_add(
			responseData.size(),
			std::memory_order_relaxed
		);
	}
	return stored;
}

bool InstrumentedCacheDatabase::prune() {
	const auto start = std::chrono::steady_clock::now();
	const bool pruned = this->m_database->prune();
	if (this->m_statistics != nullptr) {
		const uint64_t elapsedMicroseconds = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - start
			).count()
		);
		this->m_statistics->requestCachePruneCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		this->m_statistics->requestCachePruneMicroseconds.fetch_add(
			elapsedMicroseconds,
			std::memory_order_relaxed
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->requestCachePruneMaximumMicroseconds,
			elapsedMicroseconds
		);
		if (!pruned) {
			this->m_statistics->requestCachePruneFailureCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
		}
	}
	if (!pruned && this->m_failureQueue != nullptr) {
		CesiumLoadFailureRecord record;
		record.sourceInstanceId = this->m_sourceInstanceId;
		record.category = CesiumLoadFailure::Category::Cache;
		record.stage = CesiumLoadFailure::Stage::CacheMaintenance;
		record.terminal = false;
		record.message = "HTTP cache prune failed";
		this->m_failureQueue->push(std::move(record));
	}
	return pruned;
}

bool InstrumentedCacheDatabase::clearAll() {
	const bool cleared = this->m_database->clearAll();
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestCacheClearCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		if (!cleared) {
			this->m_statistics->requestCacheClearFailureCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
		}
	}
	if (!cleared && this->m_failureQueue != nullptr) {
		CesiumLoadFailureRecord record;
		record.sourceInstanceId = this->m_sourceInstanceId;
		record.category = CesiumLoadFailure::Category::Cache;
		record.stage = CesiumLoadFailure::Stage::CacheMaintenance;
		record.terminal = false;
		record.message = "HTTP cache clear failed";
		this->m_failureQueue->push(std::move(record));
	}
	return cleared;
}
