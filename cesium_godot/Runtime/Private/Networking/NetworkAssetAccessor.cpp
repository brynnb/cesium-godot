#include "Runtime/Private/Networking/NetworkAssetAccessor.h"

#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"
#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/Promise.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/Uri.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "core/config/engine.h"
#endif

namespace {
using Clock = std::chrono::steady_clock;

class EmptyAssetRequest final : public CesiumAsync::IAssetRequest {
public:
	EmptyAssetRequest(
		const std::string& method,
		const std::string& url,
		const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers
	) : m_method(method), m_url(url) {
		for (const auto& [name, value] : headers) {
			this->m_headers.emplace(name, value);
		}
	}

	const std::string& method() const override { return this->m_method; }
	const std::string& url() const override { return this->m_url; }
	const CesiumAsync::HttpHeaders& headers() const override {
		return this->m_headers;
	}
	const CesiumAsync::IAssetResponse* response() const override {
		return nullptr;
	}

private:
	std::string m_method;
	std::string m_url;
	CesiumAsync::HttpHeaders m_headers;
};

class LocalFileAssetResponse final : public CesiumAsync::IAssetResponse {
public:
	LocalFileAssetResponse(
		uint16_t statusCode,
		std::string contentType,
		std::vector<std::byte>&& data
	) : m_statusCode(statusCode),
		m_contentType(std::move(contentType)),
		m_data(std::move(data)) {
		this->m_headers.emplace("Content-Type", this->m_contentType);
	}

	uint16_t statusCode() const override { return this->m_statusCode; }
	std::string contentType() const override { return this->m_contentType; }
	const CesiumAsync::HttpHeaders& headers() const override {
		return this->m_headers;
	}
	std::span<const std::byte> data() const override { return this->m_data; }

private:
	uint16_t m_statusCode;
	std::string m_contentType;
	CesiumAsync::HttpHeaders m_headers;
	std::vector<std::byte> m_data;
};

class LocalFileAssetRequest final : public CesiumAsync::IAssetRequest {
public:
	LocalFileAssetRequest(
		std::string url,
		const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
		std::shared_ptr<LocalFileAssetResponse> response
	) : m_url(std::move(url)), m_response(std::move(response)) {
		for (const auto& [name, value] : headers) {
			this->m_headers.emplace(name, value);
		}
	}

	const std::string& method() const override { return this->m_method; }
	const std::string& url() const override { return this->m_url; }
	const CesiumAsync::HttpHeaders& headers() const override {
		return this->m_headers;
	}
	const CesiumAsync::IAssetResponse* response() const override {
		return this->m_response.get();
	}

private:
	std::string m_method = "GET";
	std::string m_url;
	CesiumAsync::HttpHeaders m_headers;
	std::shared_ptr<LocalFileAssetResponse> m_response;
};

bool is_http_url(const std::string& url) {
	return url.starts_with("http://") || url.starts_with("https://");
}

bool is_file_url(const std::string& url) {
	return url.starts_with("file://");
}

std::string content_type_for_path(const std::filesystem::path& path) {
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});
	if (extension == ".png") return "image/png";
	if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
	if (extension == ".webp") return "image/webp";
	if (extension == ".ktx2") return "image/ktx2";
	if (extension == ".json") return "application/json";
	if (extension == ".gltf") return "model/gltf+json";
	if (extension == ".glb") return "model/gltf-binary";
	return "application/octet-stream";
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
read_local_file(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& url,
	const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
	const CesiumAsync::AssetRequestOptions& options
) {
	return asyncSystem.runInWorkerThread([url, headers, options]() {
		if (options.isCancellationRequested()) {
			return std::shared_ptr<CesiumAsync::IAssetRequest>(
				std::make_shared<EmptyAssetRequest>("GET", url, headers)
			);
		}

		std::string uriPath = url.substr(std::string("file://").size());
		if (uriPath.starts_with("localhost/")) {
			uriPath = "/" + uriPath.substr(std::string("localhost/").size());
		}
		const size_t parameterStart = uriPath.find_first_of("?#");
		if (parameterStart != std::string::npos) {
			uriPath.resize(parameterStart);
		}
		// A standards-compliant local file URI is file:///absolute/path. Do
		// not silently reinterpret file://remote-host/path as a local relative
		// path; network shares need an application-provided accessor policy.
		if (uriPath.empty() || uriPath.front() != '/') {
			return std::shared_ptr<CesiumAsync::IAssetRequest>(
				std::make_shared<LocalFileAssetRequest>(
					url,
					headers,
					std::make_shared<LocalFileAssetResponse>(
						400,
						"text/plain",
						std::vector<std::byte>()
					)
				)
			);
		}

		const std::filesystem::path path(
			CesiumUtility::Uri::uriPathToNativePath(uriPath)
		);
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input) {
			return std::shared_ptr<CesiumAsync::IAssetRequest>(
				std::make_shared<LocalFileAssetRequest>(
					url,
					headers,
					std::make_shared<LocalFileAssetResponse>(
						404,
						content_type_for_path(path),
						std::vector<std::byte>()
					)
				)
			);
		}

		const std::streamoff length = input.tellg();
		if (length < 0 || static_cast<uint64_t>(length) > std::numeric_limits<size_t>::max()) {
			return std::shared_ptr<CesiumAsync::IAssetRequest>(
				std::make_shared<LocalFileAssetRequest>(
					url,
					headers,
					std::make_shared<LocalFileAssetResponse>(
						500,
						content_type_for_path(path),
						std::vector<std::byte>()
					)
				)
			);
		}

		std::vector<std::byte> data(static_cast<size_t>(length));
		input.seekg(0, std::ios::beg);
		if (!data.empty()) {
			input.read(reinterpret_cast<char*>(data.data()), length);
		}
		const uint16_t status = input || data.empty() ? 200 : 500;
		if (status != 200) data.clear();
		return std::shared_ptr<CesiumAsync::IAssetRequest>(
			std::make_shared<LocalFileAssetRequest>(
				url,
				headers,
				std::make_shared<LocalFileAssetResponse>(
					status,
					content_type_for_path(path),
					std::move(data)
				)
			)
		);
	});
}

std::string upper_ascii(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return value;
}

bool is_idempotent_read(const std::string& verb) {
	const std::string upper = upper_ascii(verb);
	return upper == "GET" || upper == "HEAD";
}

bool is_retryable_status(int32_t status) {
	return status == 0 || status == 408 || status == 425 || status == 429 ||
		status == 500 || status == 502 || status == 503 || status == 504 ||
		(status >= 520 && status <= 527);
}

bool is_success_status(int32_t status) {
	return status >= 200 && status < 400;
}

double retry_after_seconds(const CesiumAsync::IAssetResponse* response) {
	if (response == nullptr) {
		return -1.0;
	}
	const auto iterator = response->headers().find("Retry-After");
	if (iterator == response->headers().end() || iterator->second.empty()) {
		return -1.0;
	}
	try {
		size_t parsedCharacters = 0;
		const double value = std::stod(iterator->second, &parsedCharacters);
		return parsedCharacters == iterator->second.size() &&
			std::isfinite(value) && value >= 0.0
			? value
			: -1.0;
	} catch (...) {
		// HTTP-date Retry-After values require wall-clock parsing. The bounded
		// exponential policy remains safe when a server does not send delta-seconds.
		return -1.0;
	}
}

} // namespace

struct NetworkAssetAccessor::RequestState {
	RequestState(
		const CesiumAsync::AsyncSystem& asyncSystem_,
		const CesiumAsync::Promise<std::shared_ptr<CesiumAsync::IAssetRequest>>& promise_,
		const std::string& verb_,
		const std::string& url_,
		const std::vector<THeader>& headers_,
		const std::span<const std::byte>& payload_,
		const CesiumAsync::AssetRequestOptions& options_
	) : asyncSystem(asyncSystem_),
		promise(promise_),
		verb(verb_),
		url(url_),
		headers(headers_),
		payload(payload_.begin(), payload_.end()),
		options(options_),
		started(Clock::now()) {}

	CesiumAsync::AsyncSystem asyncSystem;
	CesiumAsync::Promise<std::shared_ptr<CesiumAsync::IAssetRequest>> promise;
	std::string verb;
	std::string url;
	std::vector<THeader> headers;
	std::vector<std::byte> payload;
	CesiumAsync::AssetRequestOptions options;
	Clock::time_point started;
	Clock::time_point retryAt;
	uint32_t attempt = 0;
	std::atomic_bool completed{false};
	std::shared_ptr<CesiumAsync::IAssetRequest> lastRequest;
	std::string lastException;
};

struct NetworkAssetAccessor::AttemptCompletion {
	std::shared_ptr<RequestState> state;
	std::shared_ptr<CesiumAsync::IAssetRequest> request;
	std::string exceptionMessage;
};

NetworkAssetAccessor::NetworkAssetAccessor(
	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
	const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue,
	uint64_t sourceInstanceId,
	const CesiumNetworkRetryOptions& retryOptions
) : m_statistics(statistics),
	m_failureQueue(failureQueue),
	m_sourceInstanceId(sourceInstanceId),
	m_retryOptions(retryOptions) {
	this->m_retryOptions.initialDelaySeconds = std::max(
		0.0,
		this->m_retryOptions.initialDelaySeconds
	);
	this->m_retryOptions.maximumDelaySeconds = std::max(
		this->m_retryOptions.initialDelaySeconds,
		this->m_retryOptions.maximumDelaySeconds
	);

	CesiumCurl::CurlAssetAccessorOptions options;
	const String godotVersion = Engine::get_singleton()
		->get_version_info()
		.get("string", "unknown");
	options.userAgent =
		"3D Tiles For Godot/1.0 Godot/" +
		std::string(godotVersion.utf8().get_data());
	options.requestHeaders = {
		{"x-cesium-client", "3D Tiles For Godot"},
		{"x-cesium-client-version", "1.0"},
		{"x-cesium-client-engine", godotVersion.utf8().get_data()}
	};
	// This decorator owns cancellation completion. Returning a request with no
	// response lets libcurl abort immediately without rejecting an async++
	// continuation while its cancellation callback is still unwinding.
	options.resolveCancellationAsNullResponse = true;
	// Response-less status 0 is the accessor contract this decorator uses for
	// classifying and retrying connection-level failures.
	options.resolveErrorsAsNullResponse = true;
	this->m_delegate =
		std::make_shared<CesiumCurl::CurlAssetAccessor>(options);
}

NetworkAssetAccessor::~NetworkAssetAccessor() = default;

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
NetworkAssetAccessor::get(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& url,
	const std::vector<THeader>& headers
) {
	return this->getWithOptions(asyncSystem, url, headers, {});
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
NetworkAssetAccessor::getWithOptions(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& url,
	const std::vector<THeader>& headers,
	const CesiumAsync::AssetRequestOptions& options
) {
	return this->begin_request(
		asyncSystem,
		"GET",
		url,
		headers,
		{},
		options
	);
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
NetworkAssetAccessor::request(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& verb,
	const std::string& url,
	const std::vector<THeader>& headers,
	const std::span<const std::byte>& contentPayload
) {
	return this->requestWithOptions(
		asyncSystem,
		verb,
		url,
		headers,
		contentPayload,
		{}
	);
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
NetworkAssetAccessor::requestWithOptions(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& verb,
	const std::string& url,
	const std::vector<THeader>& headers,
	const std::span<const std::byte>& contentPayload,
	const CesiumAsync::AssetRequestOptions& options
) {
	return this->begin_request(
		asyncSystem,
		verb,
		url,
		headers,
		contentPayload,
		options
	);
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
NetworkAssetAccessor::begin_request(
	const CesiumAsync::AsyncSystem& asyncSystem,
	const std::string& verb,
	const std::string& url,
	const std::vector<THeader>& headers,
	const std::span<const std::byte>& contentPayload,
	const CesiumAsync::AssetRequestOptions& options
) {
	CesiumAsync::Promise<std::shared_ptr<CesiumAsync::IAssetRequest>> promise =
		asyncSystem.createPromise<std::shared_ptr<CesiumAsync::IAssetRequest>>();
	auto future = promise.getFuture();
	auto state = std::make_shared<RequestState>(
		asyncSystem,
		promise,
		verb,
		url,
		headers,
		contentPayload,
		options
	);
	{
		std::lock_guard<std::mutex> lock(this->m_pendingMutex);
		this->m_activeRequests.push_back(state);
	}
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestCount.fetch_add(1, std::memory_order_relaxed);
		const uint64_t inFlight =
			this->m_statistics->requestInFlightCount.fetch_add(
				1,
				std::memory_order_relaxed
			) + 1;
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumRequestInFlightCount,
			inFlight
		);
	}
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			nullptr,
			"Request was canceled before its first attempt"
		);
	} else {
		this->begin_attempt(state);
	}
	return future;
}

void NetworkAssetAccessor::begin_attempt(
	const std::shared_ptr<RequestState>& state
) {
	if (state->completed.load(std::memory_order_relaxed)) {
		return;
	}
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		state->options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(state->lastRequest),
			state->lastException
		);
		return;
	}
	++state->attempt;
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestAttemptCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
	}

	try {
		auto requestFuture = upper_ascii(state->verb) == "GET"
			? (is_file_url(state->url)
				? read_local_file(
					state->asyncSystem,
					state->url,
					state->headers,
					state->options
				)
				: this->m_delegate->getWithOptions(
				state->asyncSystem,
				state->url,
				state->headers,
				state->options
				))
			: this->m_delegate->requestWithOptions(
				state->asyncSystem,
				state->verb,
				state->url,
				state->headers,
				std::span<const std::byte>(state->payload),
				state->options
			);
		auto self = this->shared_from_this();
		auto exceptionMessage = std::make_shared<std::string>();
		auto continuation = std::move(requestFuture)
			.catchImmediately([
				exceptionMessage
			](std::exception&& exception) {
				*exceptionMessage = exception.what();
				return std::shared_ptr<CesiumAsync::IAssetRequest>();
			})
			.thenImmediately([
				self,
				state,
				exceptionMessage
			](std::shared_ptr<CesiumAsync::IAssetRequest>&& request) {
				const std::string reportedException = request == nullptr
					? *exceptionMessage
					: std::string();
				self->queue_attempt_completion(AttemptCompletion{
					state,
					std::move(request),
					reportedException
				});
			});
		this->retain_attempt_continuation(std::move(continuation));
	} catch (const std::exception& exception) {
		this->handle_attempt_exception(state, exception.what());
	}
}

void NetworkAssetAccessor::retain_attempt_continuation(
	CesiumAsync::Future<void>&& continuation
) {
	std::lock_guard<std::mutex> lock(this->m_continuationMutex);
	this->m_attemptContinuations.emplace_back(std::move(continuation));
}

void NetworkAssetAccessor::reap_attempt_continuations() noexcept {
	std::lock_guard<std::mutex> lock(this->m_continuationMutex);
	auto iterator = this->m_attemptContinuations.begin();
	while (iterator != this->m_attemptContinuations.end()) {
		if (!iterator->isReady()) {
			++iterator;
			continue;
		}
		try {
			// The terminal catch above converts transport rejection to a queued
			// record. Waiting on an already-ready future consumes the successful
			// terminal task without blocking the Godot thread.
			iterator->wait();
		} catch (...) {
			// queue_attempt_completion is noexcept, so this is a final safety net
			// for an unexpected continuation failure during shutdown.
		}
		iterator = this->m_attemptContinuations.erase(iterator);
	}
}

void NetworkAssetAccessor::queue_attempt_completion(
	AttemptCompletion&& completion
) noexcept {
	std::lock_guard<std::mutex> lock(this->m_completionMutex);
	this->m_completedAttempts.emplace_back(std::move(completion));
}

void NetworkAssetAccessor::handle_completed_attempt(
	const std::shared_ptr<RequestState>& state,
	std::shared_ptr<CesiumAsync::IAssetRequest>&& request
) {
	if (state->completed.load(std::memory_order_relaxed)) {
		return;
	}
	state->lastRequest = request;
	const CesiumAsync::IAssetResponse* response =
		request != nullptr ? request->response() : nullptr;
	const int32_t status = response != nullptr
		? static_cast<int32_t>(response->statusCode())
		: 0;
	if (response != nullptr && this->m_statistics != nullptr) {
		this->m_statistics->requestResponseBytes.fetch_add(
			response->data().size(),
			std::memory_order_relaxed
		);
	}
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		state->options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(request)
		);
		return;
	}
	if (!is_http_url(state->url) || is_success_status(status)) {
		this->finish_request(
			state,
			CompletionKind::Success,
			std::move(request)
		);
		return;
	}

	const bool retryable = is_idempotent_read(state->verb) &&
		is_retryable_status(status);
	const std::string message = response == nullptr
		? "Network request completed without a response"
		: "HTTP request failed with status " + std::to_string(status);
	if (retryable && this->schedule_retry(
		state,
		status,
		message,
		retry_after_seconds(response)
	)) {
		return;
	}
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		state->options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(request)
		);
		return;
	}
	if (retryable && this->m_statistics != nullptr) {
		this->m_statistics->requestRetryExhaustedCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
	}
	this->queue_network_failure(
		state,
		status,
		message,
		retryable,
		false,
		0.0
	);
	this->finish_request(
		state,
		CompletionKind::Failure,
		std::move(request)
	);
}

void NetworkAssetAccessor::handle_attempt_exception(
	const std::shared_ptr<RequestState>& state,
	const std::string& message
) {
	if (state->completed.load(std::memory_order_relaxed)) {
		return;
	}
	state->lastException = message;
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		state->options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(state->lastRequest),
			message
		);
		return;
	}
	const bool retryable = is_http_url(state->url) &&
		is_idempotent_read(state->verb);
	if (retryable && this->schedule_retry(state, 0, message, -1.0)) {
		return;
	}
	if (this->m_cancelAll.load(std::memory_order_relaxed) ||
		state->options.isCancellationRequested()) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(state->lastRequest),
			message
		);
		return;
	}
	if (retryable && this->m_statistics != nullptr) {
		this->m_statistics->requestRetryExhaustedCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
	}
	this->queue_network_failure(
		state,
		0,
		message,
		retryable,
		false,
		0.0
	);
	this->finish_request(
		state,
		CompletionKind::Failure,
		std::move(state->lastRequest),
		message
	);
}

bool NetworkAssetAccessor::schedule_retry(
	const std::shared_ptr<RequestState>& state,
	int32_t httpStatusCode,
	const std::string& message,
	double serverDelaySeconds
) {
	if (state->attempt > this->m_retryOptions.maximumRetries) {
		return false;
	}
	const double exponent = static_cast<double>(state->attempt - 1);
	double delay = this->m_retryOptions.initialDelaySeconds *
		std::pow(2.0, exponent);
	if (serverDelaySeconds >= 0.0) {
		delay = std::max(delay, serverDelaySeconds);
	}
	delay = std::clamp(
		delay,
		0.0,
		this->m_retryOptions.maximumDelaySeconds
	);
	state->retryAt = Clock::now() + std::chrono::duration_cast<Clock::duration>(
		std::chrono::duration<double>(delay)
	);
	{
		std::lock_guard<std::mutex> lock(this->m_pendingMutex);
		if (this->m_cancelAll.load(std::memory_order_relaxed) ||
			state->options.isCancellationRequested()) {
			return false;
		}
		this->m_pendingRetries.push_back(state);
	}
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestRetryCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		const uint64_t queued =
			this->m_statistics->requestRetryQueuedCount.fetch_add(
				1,
				std::memory_order_relaxed
			) + 1;
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->maximumRequestRetryQueuedCount,
			queued
		);
	}
	this->queue_network_failure(
		state,
		httpStatusCode,
		message,
		true,
		true,
		delay
	);
	return true;
}

void NetworkAssetAccessor::finish_request(
	const std::shared_ptr<RequestState>& state,
	CompletionKind kind,
	std::shared_ptr<CesiumAsync::IAssetRequest>&& request,
	const std::string& exceptionMessage
) {
	bool expected = false;
	if (!state->completed.compare_exchange_strong(
		expected,
		true,
		std::memory_order_relaxed
	)) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(this->m_pendingMutex);
		const auto iterator = std::find(
			this->m_activeRequests.begin(),
			this->m_activeRequests.end(),
			state
		);
		if (iterator != this->m_activeRequests.end()) {
			this->m_activeRequests.erase(iterator);
		}
	}
	if (this->m_statistics != nullptr) {
		this->m_statistics->requestInFlightCount.fetch_sub(
			1,
			std::memory_order_relaxed
		);
		std::atomic<uint64_t>* completionCounter =
			kind == CompletionKind::Success
			? &this->m_statistics->requestSuccessCount
			: kind == CompletionKind::Failure
				? &this->m_statistics->requestFailureCount
				: &this->m_statistics->requestCancellationCount;
		completionCounter->fetch_add(1, std::memory_order_relaxed);
		const uint64_t elapsedMicroseconds = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				Clock::now() - state->started
			).count()
		);
		this->m_statistics->requestMicroseconds.fetch_add(
			elapsedMicroseconds,
			std::memory_order_relaxed
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->requestMaximumMicroseconds,
			elapsedMicroseconds
		);
	}
	if (request == nullptr) {
		// A failed Curl worker Future is consumed above. Resolve the accessor
		// contract with a request that has no response so Cesium's normal root
		// or tile-content path can classify it. Rejecting a second Promise from
		// inside async++'s cancellation continuation can recursively cancel that
		// same continuation before it unwinds.
		request = std::make_shared<EmptyAssetRequest>(
			state->verb,
			state->url,
			state->headers
		);
	}
	state->promise.resolve(std::move(request));
}

void NetworkAssetAccessor::queue_network_failure(
	const std::shared_ptr<RequestState>& state,
	int32_t httpStatusCode,
	const std::string& message,
	bool retryable,
	bool retryScheduled,
	double retryDelaySeconds
) const {
	if (this->m_failureQueue == nullptr) {
		return;
	}
	CesiumLoadFailureRecord record;
	record.sourceInstanceId = this->m_sourceInstanceId;
	record.category = CesiumLoadFailure::Category::Network;
	record.stage = CesiumLoadFailure::Stage::NetworkRequest;
	record.message = message;
	record.url = redact_cesium_diagnostic_url(state->url);
	record.httpStatusCode = httpStatusCode;
	record.terminal = !retryScheduled;
	record.retryable = retryable;
	record.retryScheduled = retryScheduled;
	record.attempt = static_cast<int32_t>(state->attempt);
	record.maximumAttempts = static_cast<int32_t>(
		this->m_retryOptions.maximumRetries + 1
	);
	record.retryDelaySeconds = retryDelaySeconds;
	this->m_failureQueue->push(std::move(record));
}

void NetworkAssetAccessor::tick() noexcept {
	this->m_delegate->tick();
	this->reap_attempt_continuations();
	std::vector<AttemptCompletion> completions;
	{
		std::lock_guard<std::mutex> lock(this->m_completionMutex);
		completions.swap(this->m_completedAttempts);
	}
	for (AttemptCompletion& completion : completions) {
		if (!completion.exceptionMessage.empty()) {
			this->handle_attempt_exception(
				completion.state,
				completion.exceptionMessage
			);
		} else {
			this->handle_completed_attempt(
				completion.state,
				std::move(completion.request)
			);
		}
	}
	std::vector<std::shared_ptr<RequestState>> ready;
	std::vector<std::shared_ptr<RequestState>> canceled;
	const Clock::time_point now = Clock::now();
	{
		std::lock_guard<std::mutex> lock(this->m_pendingMutex);
		auto iterator = this->m_pendingRetries.begin();
		while (iterator != this->m_pendingRetries.end()) {
			const std::shared_ptr<RequestState>& state = *iterator;
			const bool cancel = this->m_cancelAll.load(std::memory_order_relaxed) ||
				state->options.isCancellationRequested();
			if (!cancel && state->retryAt > now) {
				++iterator;
				continue;
			}
			(cancel ? canceled : ready).push_back(state);
			iterator = this->m_pendingRetries.erase(iterator);
			if (this->m_statistics != nullptr) {
				this->m_statistics->requestRetryQueuedCount.fetch_sub(
					1,
					std::memory_order_relaxed
				);
			}
		}
	}
	for (const std::shared_ptr<RequestState>& state : canceled) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(state->lastRequest),
			state->lastException
		);
	}
	for (const std::shared_ptr<RequestState>& state : ready) {
		this->begin_attempt(state);
	}
}

void NetworkAssetAccessor::cancel_all() noexcept {
	this->m_cancelAll.store(true, std::memory_order_relaxed);
	std::vector<std::shared_ptr<RequestState>> active;
	{
		std::lock_guard<std::mutex> lock(this->m_pendingMutex);
		active = this->m_activeRequests;
	}
	for (const std::shared_ptr<RequestState>& state : active) {
		this->finish_request(
			state,
			CompletionKind::Cancellation,
			std::move(state->lastRequest),
			state->lastException
		);
	}
	this->tick();
}
