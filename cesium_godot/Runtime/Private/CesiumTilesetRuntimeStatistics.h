#ifndef CESIUM_TILESET_RUNTIME_STATISTICS_H
#define CESIUM_TILESET_RUNTIME_STATISTICS_H

#include <atomic>
#include <cstdint>

struct CesiumTilesetRuntimeStatistics {
	std::atomic<uint64_t> requestCount{0};
	std::atomic<uint64_t> requestAttemptCount{0};
	std::atomic<uint64_t> requestSuccessCount{0};
	std::atomic<uint64_t> requestFailureCount{0};
	std::atomic<uint64_t> requestCancellationCount{0};
	std::atomic<uint64_t> requestRetryCount{0};
	std::atomic<uint64_t> requestRetryExhaustedCount{0};
	std::atomic<uint64_t> requestInFlightCount{0};
	std::atomic<uint64_t> maximumRequestInFlightCount{0};
	std::atomic<uint64_t> requestRetryQueuedCount{0};
	std::atomic<uint64_t> maximumRequestRetryQueuedCount{0};
	std::atomic<uint64_t> requestMicroseconds{0};
	std::atomic<uint64_t> requestMaximumMicroseconds{0};
	std::atomic<uint64_t> requestResponseBytes{0};
	std::atomic<uint64_t> decodeCount{0};
	std::atomic<uint64_t> decodeFailureCount{0};
	std::atomic<uint64_t> decodeMicroseconds{0};
	std::atomic<uint64_t> decodeMaximumMicroseconds{0};
	std::atomic<uint64_t> workerPreparationCount{0};
	std::atomic<uint64_t> workerPreparationMicroseconds{0};
	std::atomic<uint64_t> workerPreparationMaximumMicroseconds{0};
	std::atomic<uint64_t> mainThreadRealizationCount{0};
	std::atomic<uint64_t> mainThreadRealizationMicroseconds{0};
	std::atomic<uint64_t> mainThreadRealizationMaximumMicroseconds{0};
	std::atomic<uint64_t> mainThreadRealizationStepCount{0};
	std::atomic<uint64_t> mainThreadRealizationStepMicroseconds{0};
	std::atomic<uint64_t> mainThreadRealizationStepMaximumMicroseconds{0};
	std::atomic<uint64_t> incrementalRealizationYieldCount{0};
	std::atomic<uint64_t> oversizedPayloadRejectionCount{0};
	std::atomic<uint64_t> preparedGeometryBytes{0};
	std::atomic<uint64_t> preparedTextureBytes{0};
	std::atomic<uint64_t> maximumPreparedGeometryBytes{0};
	std::atomic<uint64_t> maximumPreparedTextureBytes{0};
	std::atomic<uint64_t> maximumPreparedPrimitiveGeometryBytes{0};
	std::atomic<uint64_t> maximumPreparedPrimitiveTextureBytes{0};
	std::atomic<uint64_t> sharedTextureCacheHitCount{0};
	std::atomic<uint64_t> sharedTextureCacheMissCount{0};
	std::atomic<uint64_t> liveSharedTextureCount{0};
	std::atomic<uint64_t> liveSharedTextureBytes{0};
	std::atomic<uint64_t> maximumLiveSharedTextureCount{0};
	std::atomic<uint64_t> maximumLiveSharedTextureBytes{0};
	std::atomic<uint64_t> releasedCpuTextureCount{0};
	std::atomic<uint64_t> releasedCpuTextureBytes{0};
	std::atomic<uint64_t> sharedShaderCacheHitCount{0};
	std::atomic<uint64_t> sharedShaderCacheMissCount{0};
	std::atomic<uint64_t> liveSharedShaderCount{0};
	std::atomic<uint64_t> maximumLiveSharedShaderCount{0};
	std::atomic<uint64_t> sharedModelCacheHitCount{0};
	std::atomic<uint64_t> sharedModelCacheMissCount{0};
	std::atomic<uint64_t> liveSharedModelCount{0};
	std::atomic<uint64_t> liveSharedModelGeometryBytes{0};
	std::atomic<uint64_t> liveSharedModelTextureBytes{0};
	std::atomic<uint64_t> maximumLiveSharedModelCount{0};
	std::atomic<uint64_t> maximumLiveSharedModelGeometryBytes{0};
	std::atomic<uint64_t> maximumLiveSharedModelTextureBytes{0};
	std::atomic<uint64_t> tileUnloadCount{0};
	std::atomic<uint64_t> requestCacheLookupCount{0};
	std::atomic<uint64_t> requestCacheHitCount{0};
	std::atomic<uint64_t> requestCacheMissCount{0};
	std::atomic<uint64_t> requestCacheStoreAttemptCount{0};
	std::atomic<uint64_t> requestCacheStoreSuccessCount{0};
	std::atomic<uint64_t> requestCacheStoredPayloadBytes{0};
	std::atomic<uint64_t> requestCachePruneCount{0};
	std::atomic<uint64_t> requestCachePruneFailureCount{0};
	std::atomic<uint64_t> requestCachePruneMicroseconds{0};
	std::atomic<uint64_t> requestCachePruneMaximumMicroseconds{0};
	std::atomic<uint64_t> requestCacheClearCount{0};
	std::atomic<uint64_t> requestCacheClearFailureCount{0};

	static void update_maximum(
		std::atomic<uint64_t>& target,
		uint64_t value
	) {
		uint64_t previous = target.load(std::memory_order_relaxed);
		while (
			previous < value &&
			!target.compare_exchange_weak(
				previous,
				value,
				std::memory_order_relaxed
			)
		) {
		}
	}
};

#endif // CESIUM_TILESET_RUNTIME_STATISTICS_H
