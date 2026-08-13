#ifndef NETWORK_ASSET_ACCESSOR_H
#define NETWORK_ASSET_ACCESSOR_H

#include <CesiumAsync/IAssetAccessor.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

class CesiumLoadFailureQueue;
struct CesiumTilesetRuntimeStatistics;

struct CesiumNetworkRetryOptions {
	uint32_t maximumRetries = 3;
	double initialDelaySeconds = 0.25;
	double maximumDelaySeconds = 4.0;
};

/**
 * Godot-facing construction point for Cesium Native's maintained libcurl
 * accessor, plus a non-blocking bounded retry layer for idempotent HTTP reads.
 * Delayed attempts are started by tick(); no worker or Godot thread sleeps.
 *
 * Cesium for Unreal counterparts:
 * - Source/CesiumRuntime/Public/UnrealAssetAccessor.h
 * - Source/CesiumRuntime/Private/UnrealAssetAccessor.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0.
 */
class NetworkAssetAccessor final
	: public CesiumAsync::IAssetAccessor,
	  public std::enable_shared_from_this<NetworkAssetAccessor> {
public:
	NetworkAssetAccessor(
		const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
		const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue,
		uint64_t sourceInstanceId,
		const CesiumNetworkRetryOptions& retryOptions
	);
	~NetworkAssetAccessor() override;

	CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
		const CesiumAsync::AsyncSystem& asyncSystem,
		const std::string& url,
		const std::vector<THeader>& headers = {}
	) override;

	CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
	getWithOptions(
		const CesiumAsync::AsyncSystem& asyncSystem,
		const std::string& url,
		const std::vector<THeader>& headers,
		const CesiumAsync::AssetRequestOptions& options
	) override;

	CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
		const CesiumAsync::AsyncSystem& asyncSystem,
		const std::string& verb,
		const std::string& url,
		const std::vector<THeader>& headers = std::vector<THeader>(),
		const std::span<const std::byte>& contentPayload = {}
	) override;

	CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
	requestWithOptions(
		const CesiumAsync::AsyncSystem& asyncSystem,
		const std::string& verb,
		const std::string& url,
		const std::vector<THeader>& headers,
		const std::span<const std::byte>& contentPayload,
		const CesiumAsync::AssetRequestOptions& options
	) override;

	void tick() noexcept override;
	void cancel_all() noexcept;

private:
	struct RequestState;
	struct AttemptCompletion;
	enum class CompletionKind { Success, Failure, Cancellation };

	CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
	begin_request(
		const CesiumAsync::AsyncSystem& asyncSystem,
		const std::string& verb,
		const std::string& url,
		const std::vector<THeader>& headers,
		const std::span<const std::byte>& contentPayload,
		const CesiumAsync::AssetRequestOptions& options
	);
	void begin_attempt(const std::shared_ptr<RequestState>& state);
	void retain_attempt_continuation(CesiumAsync::Future<void>&& continuation);
	void reap_attempt_continuations() noexcept;
	void queue_attempt_completion(AttemptCompletion&& completion) noexcept;
	void handle_completed_attempt(
		const std::shared_ptr<RequestState>& state,
		std::shared_ptr<CesiumAsync::IAssetRequest>&& request
	);
	void handle_attempt_exception(
		const std::shared_ptr<RequestState>& state,
		const std::string& message
	);
	bool schedule_retry(
		const std::shared_ptr<RequestState>& state,
		int32_t httpStatusCode,
		const std::string& message,
		double serverDelaySeconds
	);
	void finish_request(
		const std::shared_ptr<RequestState>& state,
		CompletionKind kind,
		std::shared_ptr<CesiumAsync::IAssetRequest>&& request,
		const std::string& exceptionMessage = std::string()
	);
	void queue_network_failure(
		const std::shared_ptr<RequestState>& state,
		int32_t httpStatusCode,
		const std::string& message,
		bool retryable,
		bool retryScheduled,
		double retryDelaySeconds
	) const;

	std::shared_ptr<CesiumAsync::IAssetAccessor> m_delegate;
	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_statistics;
	std::shared_ptr<CesiumLoadFailureQueue> m_failureQueue;
	uint64_t m_sourceInstanceId = 0;
	CesiumNetworkRetryOptions m_retryOptions;
	std::atomic_bool m_cancelAll{false};
	std::mutex m_pendingMutex;
	std::vector<std::shared_ptr<RequestState>> m_pendingRetries;
	std::vector<std::shared_ptr<RequestState>> m_activeRequests;
	std::mutex m_completionMutex;
	std::vector<AttemptCompletion> m_completedAttempts;
	std::mutex m_continuationMutex;
	std::vector<CesiumAsync::Future<void>> m_attemptContinuations;
};

#endif // NETWORK_ASSET_ACCESSOR_H
