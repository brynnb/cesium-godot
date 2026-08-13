// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterpart:
// - Source/CesiumRuntime/Private/CesiumGeocoderServiceBlueprintLibrary.cpp

#include "Runtime/Public/Geocoder/CesiumGeocoderService.h"

#include "Runtime/Private/Async/GodotTaskProcessor.h"
#include "Runtime/Private/Networking/NetworkAssetAccessor.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/GunzipAssetAccessor.h>
#include <CesiumIonClient/ApplicationData.h>
#include <CesiumIonClient/Connection.h>
#include <CesiumIonClient/Geocoder.h>
#include <CesiumIonClient/Response.h>
#include <CesiumUtility/Math.h>

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {
PackedFloat64Array make_float64_array(
	std::initializer_list<double> values
) {
	PackedFloat64Array result;
	result.resize(static_cast<int64_t>(values.size()));
	int64_t index = 0;
	for (double value : values) {
		result.set(index++, value);
	}
	return result;
}

CesiumIonClient::GeocoderProviderType native_provider(int32_t provider) {
	return static_cast<CesiumIonClient::GeocoderProviderType>(
		std::clamp(provider, 0, 2)
	);
}

CesiumIonClient::GeocoderRequestType native_request_type(int32_t type) {
	return static_cast<CesiumIonClient::GeocoderRequestType>(
		std::clamp(type, 0, 1)
	);
}

String response_error_message(
	const String& stage,
	const String& code,
	const String& message
) {
	String result = stage + String(" failed");
	if (!code.is_empty()) result += String(" [") + code + String("]");
	if (!message.is_empty()) result += String(": ") + message;
	return result;
}
} // namespace

struct CesiumGeocoderService::Runtime {
	Runtime(
		int32_t workerThreadCount,
		uint64_t sourceInstanceId,
		const CesiumNetworkRetryOptions& retryOptions
	) : taskProcessor(std::make_shared<GodotTaskProcessor>(
			static_cast<size_t>(std::max(1, workerThreadCount))
		)),
		asyncSystem(taskProcessor),
		networkAccessor(std::make_shared<NetworkAssetAccessor>(
			std::shared_ptr<CesiumTilesetRuntimeStatistics>(),
			std::shared_ptr<CesiumLoadFailureQueue>(),
			sourceInstanceId,
			retryOptions
		)),
		assetAccessor(std::make_shared<CesiumAsync::GunzipAssetAccessor>(
			networkAccessor
		)) {}

	std::shared_ptr<GodotTaskProcessor> taskProcessor;
	CesiumAsync::AsyncSystem asyncSystem;
	std::shared_ptr<NetworkAssetAccessor> networkAccessor;
	std::shared_ptr<CesiumAsync::IAssetAccessor> assetAccessor;
	std::optional<CesiumIonClient::ApplicationData> applicationData;
	bool applicationDataLoading = false;
	uint64_t applicationDataGeneration = 0;
	std::vector<SubmittedRequest> waitingRequests;
	std::unordered_map<int64_t, Ref<CesiumGeocoderRequest>> activeRequests;
	std::vector<CesiumAsync::Future<void>> continuations;
};

const String& CesiumGeocoderAttribution::get_html() const {
	return this->m_html;
}

bool CesiumGeocoderAttribution::get_show_on_screen() const {
	return this->m_showOnScreen;
}

void CesiumGeocoderAttribution::initialize(
	const String& html,
	bool showOnScreen
) {
	this->m_html = html;
	this->m_showOnScreen = showOnScreen;
}

void CesiumGeocoderAttribution::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_html"), &CesiumGeocoderAttribution::get_html);
	ClassDB::bind_method(
		D_METHOD("get_show_on_screen"),
		&CesiumGeocoderAttribution::get_show_on_screen
	);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "html", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_html");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_on_screen", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_show_on_screen");
}

const String& CesiumGeocoderFeature::get_display_name() const {
	return this->m_displayName;
}

Vector3 CesiumGeocoderFeature::get_longitude_latitude_height() const {
	return Vector3(
		this->m_longitudeDegrees,
		this->m_latitudeDegrees,
		this->m_heightMeters
	);
}

PackedFloat64Array
CesiumGeocoderFeature::get_longitude_latitude_height_components() const {
	return make_float64_array({
		this->m_longitudeDegrees,
		this->m_latitudeDegrees,
		this->m_heightMeters,
	});
}

PackedFloat64Array CesiumGeocoderFeature::get_globe_rectangle_components() const {
	return make_float64_array({
		this->m_westDegrees,
		this->m_southDegrees,
		this->m_eastDegrees,
		this->m_northDegrees,
	});
}

bool CesiumGeocoderFeature::get_is_point() const {
	return this->m_isPoint;
}

void CesiumGeocoderFeature::initialize(
	const String& displayName,
	double longitudeDegrees,
	double latitudeDegrees,
	double heightMeters,
	double westDegrees,
	double southDegrees,
	double eastDegrees,
	double northDegrees,
	bool isPoint
) {
	this->m_displayName = displayName;
	this->m_longitudeDegrees = longitudeDegrees;
	this->m_latitudeDegrees = latitudeDegrees;
	this->m_heightMeters = heightMeters;
	this->m_westDegrees = westDegrees;
	this->m_southDegrees = southDegrees;
	this->m_eastDegrees = eastDegrees;
	this->m_northDegrees = northDegrees;
	this->m_isPoint = isPoint;
}

void CesiumGeocoderFeature::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_display_name"), &CesiumGeocoderFeature::get_display_name);
	ClassDB::bind_method(D_METHOD("get_longitude_latitude_height"), &CesiumGeocoderFeature::get_longitude_latitude_height);
	ClassDB::bind_method(D_METHOD("get_longitude_latitude_height_components"), &CesiumGeocoderFeature::get_longitude_latitude_height_components);
	ClassDB::bind_method(D_METHOD("get_globe_rectangle_components"), &CesiumGeocoderFeature::get_globe_rectangle_components);
	ClassDB::bind_method(D_METHOD("get_is_point"), &CesiumGeocoderFeature::get_is_point);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "display_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_display_name");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "longitude_latitude_height", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_longitude_latitude_height");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "longitude_latitude_height_components", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_longitude_latitude_height_components");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "globe_rectangle_components", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_globe_rectangle_components");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_point", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_is_point");
}

Array CesiumGeocoderResult::get_attributions() const {
	return this->m_attributions.duplicate();
}

Array CesiumGeocoderResult::get_features() const {
	return this->m_features.duplicate();
}

void CesiumGeocoderResult::initialize(
	const Array& attributions,
	const Array& features
) {
	this->m_attributions = attributions.duplicate();
	this->m_features = features.duplicate();
}

void CesiumGeocoderResult::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_attributions"), &CesiumGeocoderResult::get_attributions);
	ClassDB::bind_method(D_METHOD("get_features"), &CesiumGeocoderResult::get_features);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "attributions", PROPERTY_HINT_ARRAY_TYPE, "CesiumGeocoderAttribution", PROPERTY_USAGE_NONE), "", "get_attributions");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "features", PROPERTY_HINT_ARRAY_TYPE, "CesiumGeocoderFeature", PROPERTY_USAGE_NONE), "", "get_features");
}

int64_t CesiumGeocoderRequest::get_request_id() const {
	return this->m_requestId;
}

int32_t CesiumGeocoderRequest::get_status() const {
	return static_cast<int32_t>(this->m_status);
}

bool CesiumGeocoderRequest::is_finished() const {
	return this->m_status != Pending;
}

bool CesiumGeocoderRequest::is_cancelled() const {
	return this->m_status == Cancelled;
}

int32_t CesiumGeocoderRequest::get_provider() const {
	return this->m_provider;
}

int32_t CesiumGeocoderRequest::get_request_type() const {
	return this->m_requestType;
}

const String& CesiumGeocoderRequest::get_query() const {
	return this->m_query;
}

Ref<CesiumGeocoderResult> CesiumGeocoderRequest::get_result() const {
	return this->m_result;
}

int32_t CesiumGeocoderRequest::get_http_status_code() const {
	return this->m_httpStatusCode;
}

const String& CesiumGeocoderRequest::get_error_code() const {
	return this->m_errorCode;
}

const String& CesiumGeocoderRequest::get_error_message() const {
	return this->m_errorMessage;
}

void CesiumGeocoderRequest::cancel() {
	if (this->m_status != Pending) return;
	CesiumGeocoderService* service = Object::cast_to<CesiumGeocoderService>(
		ObjectDB::get_instance(this->m_service)
	);
	if (service != nullptr) {
		service->cancel_request(this->m_requestId);
	} else {
		this->cancel_from_service();
	}
}

void CesiumGeocoderRequest::initialize(
	const ObjectID& service,
	int64_t requestId,
	int32_t provider,
	int32_t requestType,
	const String& query
) {
	this->m_service = service;
	this->m_requestId = requestId;
	this->m_provider = provider;
	this->m_requestType = requestType;
	this->m_query = query;
}

void CesiumGeocoderRequest::complete(
	const Ref<CesiumGeocoderResult>& result,
	int32_t httpStatusCode
) {
	if (this->m_status != Pending) return;
	this->m_result = result;
	this->m_httpStatusCode = httpStatusCode;
	this->m_status = Completed;
	this->emit_signal("completed", this->m_result);
}

void CesiumGeocoderRequest::fail(
	const String& errorCode,
	const String& errorMessage,
	int32_t httpStatusCode
) {
	if (this->m_status != Pending) return;
	this->m_errorCode = errorCode;
	this->m_errorMessage = errorMessage;
	this->m_httpStatusCode = httpStatusCode;
	this->m_status = Failed;
	this->emit_signal(
		"failed",
		this->m_errorCode,
		this->m_errorMessage,
		this->m_httpStatusCode
	);
}

void CesiumGeocoderRequest::cancel_from_service() {
	if (this->m_status != Pending) return;
	this->m_status = Cancelled;
	this->emit_signal("cancelled");
}

void CesiumGeocoderRequest::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_request_id"), &CesiumGeocoderRequest::get_request_id);
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumGeocoderRequest::get_status);
	ClassDB::bind_method(D_METHOD("is_finished"), &CesiumGeocoderRequest::is_finished);
	ClassDB::bind_method(D_METHOD("is_cancelled"), &CesiumGeocoderRequest::is_cancelled);
	ClassDB::bind_method(D_METHOD("get_provider"), &CesiumGeocoderRequest::get_provider);
	ClassDB::bind_method(D_METHOD("get_request_type"), &CesiumGeocoderRequest::get_request_type);
	ClassDB::bind_method(D_METHOD("get_query"), &CesiumGeocoderRequest::get_query);
	ClassDB::bind_method(D_METHOD("get_result"), &CesiumGeocoderRequest::get_result);
	ClassDB::bind_method(D_METHOD("get_http_status_code"), &CesiumGeocoderRequest::get_http_status_code);
	ClassDB::bind_method(D_METHOD("get_error_code"), &CesiumGeocoderRequest::get_error_code);
	ClassDB::bind_method(D_METHOD("get_error_message"), &CesiumGeocoderRequest::get_error_message);
	ClassDB::bind_method(D_METHOD("cancel"), &CesiumGeocoderRequest::cancel);
	BIND_ENUM_CONSTANT(Pending);
	BIND_ENUM_CONSTANT(Completed);
	BIND_ENUM_CONSTANT(Failed);
	BIND_ENUM_CONSTANT(Cancelled);
	ADD_SIGNAL(MethodInfo("completed", PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_RESOURCE_TYPE, "CesiumGeocoderResult")));
	ADD_SIGNAL(MethodInfo("failed", PropertyInfo(Variant::STRING, "error_code"), PropertyInfo(Variant::STRING, "error_message"), PropertyInfo(Variant::INT, "http_status_code")));
	ADD_SIGNAL(MethodInfo("cancelled"));
	ADD_PROPERTY(PropertyInfo(Variant::INT, "request_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_request_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "provider", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_provider");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "request_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_request_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "query", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_query");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_RESOURCE_TYPE, "CesiumGeocoderResult", PROPERTY_USAGE_NONE), "", "get_result");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "http_status_code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_http_status_code");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_code", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_error_code");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "error_message", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_error_message");
}

CesiumGeocoderService::CesiumGeocoderService() {
	this->set_process(false);
}

CesiumGeocoderService::~CesiumGeocoderService() {
	this->shutdown_runtime();
}

void CesiumGeocoderService::_enter_tree() {
	this->set_process(true);
}

void CesiumGeocoderService::_exit_tree() {
	this->set_process(false);
	this->shutdown_runtime();
}

void CesiumGeocoderService::_process(double) {
	if (this->m_runtime == nullptr) return;
	this->m_runtime->networkAccessor->tick();
	this->m_runtime->asyncSystem.dispatchMainThreadTasks();
	this->prune_continuations();
	if (
		this->m_runtimeRecreationPending &&
		this->m_runtime->activeRequests.empty() &&
		this->m_runtime->waitingRequests.empty() &&
		this->m_runtime->continuations.empty()
	) {
		this->finish_runtime_recreation();
	}
}

void CesiumGeocoderService::set_access_token(const String& value) {
	this->m_accessToken = value;
}

const String& CesiumGeocoderService::get_access_token() const {
	return this->m_accessToken;
}

String CesiumGeocoderService::normalized_api_url(const String& value) const {
	String result = value.strip_edges();
	if (!result.is_empty() && !result.ends_with("/")) result += "/";
	return result;
}

void CesiumGeocoderService::set_api_url(const String& value) {
	const String normalized = this->normalized_api_url(value);
	if (this->m_apiUrl == normalized) return;
	this->m_apiUrl = normalized;
	if (this->m_runtime == nullptr) return;
	this->m_runtimeRecreationPending = true;
	this->cancel_runtime_requests();
	this->m_runtime->networkAccessor->cancel_all();
	this->m_runtime->networkAccessor->tick();
	if (this->m_runtime->continuations.empty()) {
		this->finish_runtime_recreation();
	}
}

const String& CesiumGeocoderService::get_api_url() const {
	return this->m_apiUrl;
}

void CesiumGeocoderService::set_worker_thread_count(int32_t value) {
	const int32_t bounded = std::clamp(value, 1, 16);
	if (this->m_workerThreadCount == bounded) return;
	this->m_workerThreadCount = bounded;
	this->recreate_runtime_if_idle();
}

int32_t CesiumGeocoderService::get_worker_thread_count() const {
	return this->m_workerThreadCount;
}

void CesiumGeocoderService::set_maximum_network_retries(int32_t value) {
	const int32_t bounded = std::clamp(value, 0, 10);
	if (this->m_maximumNetworkRetries == bounded) return;
	this->m_maximumNetworkRetries = bounded;
	this->recreate_runtime_if_idle();
}

int32_t CesiumGeocoderService::get_maximum_network_retries() const {
	return this->m_maximumNetworkRetries;
}

void CesiumGeocoderService::set_network_retry_initial_delay_seconds(
	double value
) {
	const double bounded = std::clamp(value, 0.0, 60.0);
	if (this->m_networkRetryInitialDelaySeconds == bounded) return;
	this->m_networkRetryInitialDelaySeconds = bounded;
	if (this->m_networkRetryMaximumDelaySeconds < bounded) {
		this->m_networkRetryMaximumDelaySeconds = bounded;
	}
	this->recreate_runtime_if_idle();
}

double CesiumGeocoderService::get_network_retry_initial_delay_seconds() const {
	return this->m_networkRetryInitialDelaySeconds;
}

void CesiumGeocoderService::set_network_retry_maximum_delay_seconds(
	double value
) {
	const double bounded = std::clamp(value, 0.0, 300.0);
	const double normalized = std::max(
		bounded,
		this->m_networkRetryInitialDelaySeconds
	);
	if (this->m_networkRetryMaximumDelaySeconds == normalized) return;
	this->m_networkRetryMaximumDelaySeconds = normalized;
	this->recreate_runtime_if_idle();
}

double CesiumGeocoderService::get_network_retry_maximum_delay_seconds() const {
	return this->m_networkRetryMaximumDelaySeconds;
}

void CesiumGeocoderService::ensure_runtime() {
	if (this->m_runtime != nullptr) return;
	this->m_runtimeRecreationPending = false;
	CesiumNetworkRetryOptions retryOptions;
	retryOptions.maximumRetries = static_cast<uint32_t>(
		this->m_maximumNetworkRetries
	);
	retryOptions.initialDelaySeconds = this->m_networkRetryInitialDelaySeconds;
	retryOptions.maximumDelaySeconds = this->m_networkRetryMaximumDelaySeconds;
	this->m_runtime = std::make_unique<Runtime>(
		this->m_workerThreadCount,
		static_cast<uint64_t>(this->get_instance_id()),
		retryOptions
	);
}

Ref<CesiumGeocoderRequest> CesiumGeocoderService::geocode(
	const String& query,
	int32_t provider,
	int32_t requestType
) {
	Ref<CesiumGeocoderRequest> request;
	request.instantiate();
	const int32_t boundedProvider = std::clamp(provider, 0, 2);
	const int32_t boundedType = std::clamp(requestType, 0, 1);
	const int64_t requestId = this->m_nextRequestId++;
	request->initialize(
		ObjectID(this->get_instance_id()),
		requestId,
		boundedProvider,
		boundedType,
		query
	);

	if (query.strip_edges().is_empty()) {
		request->fail("InvalidQuery", "The geocoder query is empty.", 0);
		return request;
	}
	if (this->m_apiUrl.is_empty()) {
		request->fail("InvalidApiUrl", "The Cesium ion API URL is empty.", 0);
		return request;
	}
	if (this->m_shutdownInProgress) {
		request->fail(
			"ServiceShuttingDown",
			"The geocoder service is shutting down.",
			0
		);
		return request;
	}

	SubmittedRequest submitted;
	submitted.request = request;
	submitted.accessToken = this->m_accessToken;
	submitted.query = query;
	submitted.provider = boundedProvider;
	submitted.requestType = boundedType;
	if (this->m_runtimeRecreationPending && this->m_runtime != nullptr) {
		this->m_deferredRequests.emplace_back(std::move(submitted));
		return request;
	}
	this->submit_request(std::move(submitted));
	return request;
}

void CesiumGeocoderService::submit_request(SubmittedRequest&& submitted) {
	if (submitted.request.is_null() || submitted.request->is_finished()) return;
	this->ensure_runtime();
	const int64_t requestId = submitted.request->get_request_id();
	this->m_runtime->activeRequests.emplace(requestId, submitted.request);
	if (this->m_runtime->applicationData) {
		this->start_geocode_request(
			requestId,
			submitted.accessToken,
			submitted.query,
			submitted.provider,
			submitted.requestType
		);
	} else {
		this->m_runtime->waitingRequests.emplace_back(std::move(submitted));
		if (!this->m_runtime->applicationDataLoading) {
			this->start_application_data_request();
		}
	}
}

Ref<CesiumGeocoderRequest> CesiumGeocoderService::search(
	const String& query,
	int32_t provider
) {
	return this->geocode(query, provider, Search);
}

Ref<CesiumGeocoderRequest> CesiumGeocoderService::autocomplete(
	const String& query,
	int32_t provider
) {
	return this->geocode(query, provider, Autocomplete);
}

void CesiumGeocoderService::start_application_data_request() {
	if (this->m_runtime == nullptr) return;
	this->m_runtime->applicationDataLoading = true;
	const uint64_t generation = ++this->m_runtime->applicationDataGeneration;
	const ObjectID serviceId(this->get_instance_id());
	const std::string apiUrl = this->m_apiUrl.utf8().get_data();
	auto continuation = CesiumIonClient::Connection::appData(
		this->m_runtime->asyncSystem,
		this->m_runtime->assetAccessor,
		apiUrl
	).thenInMainThread([
		serviceId,
		generation
	](CesiumIonClient::Response<CesiumIonClient::ApplicationData>&& response) {
		CesiumGeocoderService* service = Object::cast_to<CesiumGeocoderService>(
			ObjectDB::get_instance(serviceId)
		);
		if (service != nullptr) {
			service->handle_application_data_response(
				generation,
				std::move(response)
			);
		}
	}).catchInMainThread([serviceId, generation](std::exception&& exception) {
		CesiumGeocoderService* service = Object::cast_to<CesiumGeocoderService>(
			ObjectDB::get_instance(serviceId)
		);
		if (service != nullptr && service->m_runtime != nullptr &&
			service->m_runtime->applicationDataGeneration == generation) {
			service->m_runtime->applicationDataLoading = false;
			service->fail_waiting_requests(
				"Exception",
				response_error_message("ion appData request", "Exception", exception.what()),
				0
			);
		}
	});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumGeocoderService::handle_application_data_response(
	uint64_t generation,
	CesiumIonClient::Response<CesiumIonClient::ApplicationData>&& response
) {
	if (this->m_runtime == nullptr ||
		this->m_runtime->applicationDataGeneration != generation) return;
	this->m_runtime->applicationDataLoading = false;
	if (!response.value) {
		this->fail_waiting_requests(
			response.errorCode.c_str(),
			response_error_message(
				"ion appData request",
				response.errorCode.c_str(),
				response.errorMessage.c_str()
			),
			static_cast<int32_t>(response.httpStatusCode)
		);
		return;
	}
	this->m_runtime->applicationData = std::move(*response.value);
	std::vector<SubmittedRequest> waiting;
	waiting.swap(this->m_runtime->waitingRequests);
	for (const SubmittedRequest& pending : waiting) {
		const int64_t requestId = pending.request->get_request_id();
		if (this->m_runtime->activeRequests.contains(requestId)) {
			this->start_geocode_request(
				requestId,
				pending.accessToken,
				pending.query,
				pending.provider,
				pending.requestType
			);
		}
	}
}

void CesiumGeocoderService::start_geocode_request(
	int64_t requestId,
	const String& accessToken,
	const String& query,
	int32_t provider,
	int32_t requestType
) {
	if (this->m_runtime == nullptr || !this->m_runtime->applicationData ||
		!this->m_runtime->activeRequests.contains(requestId)) return;

	CesiumIonClient::Connection connection(
		this->m_runtime->asyncSystem,
		this->m_runtime->assetAccessor,
		accessToken.utf8().get_data(),
		*this->m_runtime->applicationData,
		this->m_apiUrl.utf8().get_data()
	);
	const ObjectID serviceId(this->get_instance_id());
	auto continuation = connection.geocode(
		native_provider(provider),
		native_request_type(requestType),
		query.utf8().get_data()
	).thenInMainThread([
		serviceId,
		requestId
	](CesiumIonClient::Response<CesiumIonClient::GeocoderResult>&& response) {
		CesiumGeocoderService* service = Object::cast_to<CesiumGeocoderService>(
			ObjectDB::get_instance(serviceId)
		);
		if (service != nullptr) {
			service->handle_geocode_response(requestId, std::move(response));
		}
	}).catchInMainThread([serviceId, requestId](std::exception&& exception) {
		CesiumGeocoderService* service = Object::cast_to<CesiumGeocoderService>(
			ObjectDB::get_instance(serviceId)
		);
		if (service != nullptr) {
			service->handle_request_exception(requestId, exception.what());
		}
	});
	this->m_runtime->continuations.emplace_back(std::move(continuation));
}

void CesiumGeocoderService::handle_geocode_response(
	int64_t requestId,
	CesiumIonClient::Response<CesiumIonClient::GeocoderResult>&& response
) {
	if (this->m_runtime == nullptr) return;
	auto iterator = this->m_runtime->activeRequests.find(requestId);
	if (iterator == this->m_runtime->activeRequests.end()) return;
	const Ref<CesiumGeocoderRequest> request = iterator->second;
	this->m_runtime->activeRequests.erase(iterator);
	if (!response.value) {
		request->fail(
			response.errorCode.c_str(),
			response_error_message(
				"Geocoder request",
				response.errorCode.c_str(),
				response.errorMessage.c_str()
			),
			static_cast<int32_t>(response.httpStatusCode)
		);
		return;
	}

	Array attributions;
	attributions.resize(static_cast<int64_t>(response.value->attributions.size()));
	for (size_t index = 0; index < response.value->attributions.size(); ++index) {
		const CesiumIonClient::GeocoderAttribution& nativeAttribution =
			response.value->attributions[index];
		Ref<CesiumGeocoderAttribution> attribution;
		attribution.instantiate();
		attribution->initialize(
			nativeAttribution.html.c_str(),
			nativeAttribution.showOnScreen
		);
		attributions[static_cast<int64_t>(index)] = attribution;
	}

	Array features;
	features.resize(static_cast<int64_t>(response.value->features.size()));
	for (size_t index = 0; index < response.value->features.size(); ++index) {
		const CesiumIonClient::GeocoderFeature& nativeFeature =
			response.value->features[index];
		const CesiumGeospatial::Cartographic center =
			nativeFeature.getCartographic();
		const CesiumGeospatial::GlobeRectangle rectangle =
			nativeFeature.getGlobeRectangle();
		const CesiumGeospatial::Cartographic southwest = rectangle.getSouthwest();
		const CesiumGeospatial::Cartographic northeast = rectangle.getNortheast();
		Ref<CesiumGeocoderFeature> feature;
		feature.instantiate();
		feature->initialize(
			nativeFeature.displayName.c_str(),
			CesiumUtility::Math::radiansToDegrees(center.longitude),
			CesiumUtility::Math::radiansToDegrees(center.latitude),
			center.height,
			CesiumUtility::Math::radiansToDegrees(southwest.longitude),
			CesiumUtility::Math::radiansToDegrees(southwest.latitude),
			CesiumUtility::Math::radiansToDegrees(northeast.longitude),
			CesiumUtility::Math::radiansToDegrees(northeast.latitude),
			std::holds_alternative<CesiumGeospatial::Cartographic>(
				nativeFeature.destination
			)
		);
		features[static_cast<int64_t>(index)] = feature;
	}

	Ref<CesiumGeocoderResult> result;
	result.instantiate();
	result->initialize(attributions, features);
	request->complete(result, static_cast<int32_t>(response.httpStatusCode));
}

void CesiumGeocoderService::handle_request_exception(
	int64_t requestId,
	const String& message
) {
	if (this->m_runtime == nullptr) return;
	auto iterator = this->m_runtime->activeRequests.find(requestId);
	if (iterator == this->m_runtime->activeRequests.end()) return;
	const Ref<CesiumGeocoderRequest> request = iterator->second;
	this->m_runtime->activeRequests.erase(iterator);
	request->fail(
		"Exception",
		response_error_message("Geocoder request", "Exception", message),
		0
	);
}

void CesiumGeocoderService::fail_waiting_requests(
	const String& errorCode,
	const String& errorMessage,
	int32_t httpStatusCode
) {
	if (this->m_runtime == nullptr) return;
	std::vector<SubmittedRequest> waiting;
	waiting.swap(this->m_runtime->waitingRequests);
	for (const SubmittedRequest& pending : waiting) {
		const int64_t requestId = pending.request->get_request_id();
		auto iterator = this->m_runtime->activeRequests.find(requestId);
		if (iterator == this->m_runtime->activeRequests.end()) continue;
		const Ref<CesiumGeocoderRequest> request = iterator->second;
		this->m_runtime->activeRequests.erase(iterator);
		request->fail(errorCode, errorMessage, httpStatusCode);
	}
}

void CesiumGeocoderService::cancel_request(int64_t requestId) {
	if (this->m_runtime != nullptr) {
		auto iterator = this->m_runtime->activeRequests.find(requestId);
		if (iterator != this->m_runtime->activeRequests.end()) {
			const Ref<CesiumGeocoderRequest> request = iterator->second;
			this->m_runtime->activeRequests.erase(iterator);
			this->m_runtime->waitingRequests.erase(
				std::remove_if(
					this->m_runtime->waitingRequests.begin(),
					this->m_runtime->waitingRequests.end(),
					[requestId](const SubmittedRequest& pending) {
						return pending.request->get_request_id() == requestId;
					}
				),
				this->m_runtime->waitingRequests.end()
			);
			request->cancel_from_service();
			return;
		}
	}
	for (auto deferred = this->m_deferredRequests.begin();
		deferred != this->m_deferredRequests.end(); ++deferred) {
		if (deferred->request->get_request_id() != requestId) continue;
		const Ref<CesiumGeocoderRequest> request = deferred->request;
		this->m_deferredRequests.erase(deferred);
		request->cancel_from_service();
		return;
	}
}

void CesiumGeocoderService::cancel_runtime_requests() {
	if (this->m_runtime == nullptr) return;
	std::vector<Ref<CesiumGeocoderRequest>> requests;
	requests.reserve(this->m_runtime->activeRequests.size());
	for (const auto& entry : this->m_runtime->activeRequests) {
		requests.emplace_back(entry.second);
	}
	this->m_runtime->activeRequests.clear();
	this->m_runtime->waitingRequests.clear();
	for (const Ref<CesiumGeocoderRequest>& request : requests) {
		request->cancel_from_service();
	}
}

void CesiumGeocoderService::cancel_all() {
	this->cancel_runtime_requests();
	std::vector<SubmittedRequest> deferred;
	deferred.swap(this->m_deferredRequests);
	for (const SubmittedRequest& submitted : deferred) {
		submitted.request->cancel_from_service();
	}
}

int32_t CesiumGeocoderService::get_pending_request_count() const {
	const size_t activeCount = this->m_runtime == nullptr
		? 0
		: this->m_runtime->activeRequests.size();
	return static_cast<int32_t>(activeCount + this->m_deferredRequests.size());
}

Dictionary CesiumGeocoderService::get_configuration() const {
	Dictionary result;
	result["api_url"] = this->m_apiUrl;
	result["access_token_configured"] = !this->m_accessToken.is_empty();
	result["worker_thread_count"] = this->m_workerThreadCount;
	result["maximum_network_retries"] = this->m_maximumNetworkRetries;
	result["network_retry_initial_delay_seconds"] = this->m_networkRetryInitialDelaySeconds;
	result["network_retry_maximum_delay_seconds"] = this->m_networkRetryMaximumDelaySeconds;
	result["pending_request_count"] = this->get_pending_request_count();
	result["deferred_request_count"] = static_cast<int32_t>(
		this->m_deferredRequests.size()
	);
	result["runtime_recreation_pending"] = this->m_runtimeRecreationPending;
	result["application_data_cached"] = this->m_runtime != nullptr &&
		this->m_runtime->applicationData.has_value();
	return result;
}

void CesiumGeocoderService::prune_continuations() {
	if (this->m_runtime == nullptr) return;
	auto iterator = this->m_runtime->continuations.begin();
	while (iterator != this->m_runtime->continuations.end()) {
		if (!iterator->isReady()) {
			++iterator;
			continue;
		}
		iterator->wait();
		iterator = this->m_runtime->continuations.erase(iterator);
	}
}

void CesiumGeocoderService::recreate_runtime_if_idle() {
	if (this->m_runtime == nullptr) {
		this->m_runtimeRecreationPending = false;
		return;
	}
	if (
		this->m_runtime->activeRequests.empty() &&
		this->m_runtime->waitingRequests.empty() &&
		this->m_runtime->continuations.empty()
	) {
		this->finish_runtime_recreation();
		return;
	}
	// A terminal request signal may set these properties while its Native
	// continuation is still executing. Defer teardown until _process has
	// pruned that continuation instead of waiting on the current callback.
	this->m_runtimeRecreationPending = true;
}

void CesiumGeocoderService::finish_runtime_recreation() {
	std::vector<SubmittedRequest> deferred;
	deferred.swap(this->m_deferredRequests);
	this->shutdown_runtime();
	for (SubmittedRequest& submitted : deferred) {
		this->submit_request(std::move(submitted));
	}
}

void CesiumGeocoderService::shutdown_runtime() {
	this->m_runtimeRecreationPending = false;
	this->m_shutdownInProgress = true;
	this->cancel_all();
	if (this->m_runtime == nullptr) {
		this->m_shutdownInProgress = false;
		return;
	}
	this->m_runtime->networkAccessor->cancel_all();
	this->m_runtime->networkAccessor->tick();
	// Keep the explicitly-owned task processor alive on the Godot thread until
	// all canceled high-level operations release their scheduler references. If
	// the last reference were instead released by one of its own workers, the
	// processor would attempt to join that worker from itself.
	std::vector<CesiumAsync::Future<void>> continuations = std::move(
		this->m_runtime->continuations
	);
	for (CesiumAsync::Future<void>& continuation : continuations) {
		this->m_runtime->networkAccessor->tick();
		continuation.waitInMainThread();
	}
	this->m_runtime->networkAccessor->tick();
	this->m_runtime->asyncSystem.dispatchMainThreadTasks();
	// waitInMainThread may observe completion just before the worker that
	// resolved the promise releases its final scheduler reference. Stop and
	// join the explicitly-owned workers here, while the processor is still
	// guaranteed to be owned by the Godot thread. Any final continuation that
	// attempts to schedule after this point runs synchronously and cannot make
	// the processor destroy itself from one of its own workers.
	this->m_runtime->taskProcessor->shutdown();
	this->m_runtime.reset();
	this->m_shutdownInProgress = false;
}

void CesiumGeocoderService::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_access_token", "value"), &CesiumGeocoderService::set_access_token);
	ClassDB::bind_method(D_METHOD("get_access_token"), &CesiumGeocoderService::get_access_token);
	ClassDB::bind_method(D_METHOD("set_api_url", "value"), &CesiumGeocoderService::set_api_url);
	ClassDB::bind_method(D_METHOD("get_api_url"), &CesiumGeocoderService::get_api_url);
	ClassDB::bind_method(D_METHOD("set_worker_thread_count", "value"), &CesiumGeocoderService::set_worker_thread_count);
	ClassDB::bind_method(D_METHOD("get_worker_thread_count"), &CesiumGeocoderService::get_worker_thread_count);
	ClassDB::bind_method(D_METHOD("set_maximum_network_retries", "value"), &CesiumGeocoderService::set_maximum_network_retries);
	ClassDB::bind_method(D_METHOD("get_maximum_network_retries"), &CesiumGeocoderService::get_maximum_network_retries);
	ClassDB::bind_method(D_METHOD("set_network_retry_initial_delay_seconds", "value"), &CesiumGeocoderService::set_network_retry_initial_delay_seconds);
	ClassDB::bind_method(D_METHOD("get_network_retry_initial_delay_seconds"), &CesiumGeocoderService::get_network_retry_initial_delay_seconds);
	ClassDB::bind_method(D_METHOD("set_network_retry_maximum_delay_seconds", "value"), &CesiumGeocoderService::set_network_retry_maximum_delay_seconds);
	ClassDB::bind_method(D_METHOD("get_network_retry_maximum_delay_seconds"), &CesiumGeocoderService::get_network_retry_maximum_delay_seconds);
	ClassDB::bind_method(D_METHOD("geocode", "query", "provider", "request_type"), &CesiumGeocoderService::geocode, DEFVAL(Default), DEFVAL(Search));
	ClassDB::bind_method(D_METHOD("search", "query", "provider"), &CesiumGeocoderService::search, DEFVAL(Default));
	ClassDB::bind_method(D_METHOD("autocomplete", "query", "provider"), &CesiumGeocoderService::autocomplete, DEFVAL(Default));
	ClassDB::bind_method(D_METHOD("cancel_request", "request_id"), &CesiumGeocoderService::cancel_request);
	ClassDB::bind_method(D_METHOD("cancel_all"), &CesiumGeocoderService::cancel_all);
	ClassDB::bind_method(D_METHOD("get_pending_request_count"), &CesiumGeocoderService::get_pending_request_count);
	ClassDB::bind_method(D_METHOD("get_configuration"), &CesiumGeocoderService::get_configuration);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "access_token", PROPERTY_HINT_PASSWORD), "set_access_token", "get_access_token");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "api_url"), "set_api_url", "get_api_url");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "worker_thread_count", PROPERTY_HINT_RANGE, "1,16,1"), "set_worker_thread_count", "get_worker_thread_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_network_retries", PROPERTY_HINT_RANGE, "0,10,1"), "set_maximum_network_retries", "get_maximum_network_retries");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "network_retry_initial_delay_seconds", PROPERTY_HINT_RANGE, "0,60,0.01,or_greater"), "set_network_retry_initial_delay_seconds", "get_network_retry_initial_delay_seconds");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "network_retry_maximum_delay_seconds", PROPERTY_HINT_RANGE, "0,300,0.01,or_greater"), "set_network_retry_maximum_delay_seconds", "get_network_retry_maximum_delay_seconds");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pending_request_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_pending_request_count");
	BIND_ENUM_CONSTANT(Google);
	BIND_ENUM_CONSTANT(Bing);
	BIND_ENUM_CONSTANT(Default);
	BIND_ENUM_CONSTANT(Search);
	BIND_ENUM_CONSTANT(Autocomplete);
}
