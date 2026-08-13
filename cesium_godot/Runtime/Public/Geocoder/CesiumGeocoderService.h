// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterpart:
// - Source/CesiumRuntime/Public/CesiumGeocoderServiceBlueprintLibrary.h
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GEOCODER_SERVICE_H
#define CESIUM_GEOCODER_SERVICE_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/object/ref_counted.h"
#include "core/io/resource.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/packed_float64_array.h"
#include "scene/main/node.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <memory>
#include <vector>

namespace CesiumIonClient {
struct ApplicationData;
struct GeocoderResult;
template <typename T> struct Response;
} // namespace CesiumIonClient

/** A copied attribution returned by the ion geocoder. */
class CesiumGeocoderAttribution : public Resource {
	GDCLASS(CesiumGeocoderAttribution, Resource)

public:
	const String& get_html() const;
	bool get_show_on_screen() const;
	void initialize(const String& html, bool showOnScreen);

protected:
	static void _bind_methods();

private:
	String m_html;
	bool m_showOnScreen = false;
};

/** A copied geocoder destination with an authoritative float64 representation. */
class CesiumGeocoderFeature : public Resource {
	GDCLASS(CesiumGeocoderFeature, Resource)

public:
	const String& get_display_name() const;
	Vector3 get_longitude_latitude_height() const;
	PackedFloat64Array get_longitude_latitude_height_components() const;
	PackedFloat64Array get_globe_rectangle_components() const;
	bool get_is_point() const;

	void initialize(
		const String& displayName,
		double longitudeDegrees,
		double latitudeDegrees,
		double heightMeters,
		double westDegrees,
		double southDegrees,
		double eastDegrees,
		double northDegrees,
		bool isPoint
	);

protected:
	static void _bind_methods();

private:
	String m_displayName;
	double m_longitudeDegrees = 0.0;
	double m_latitudeDegrees = 0.0;
	double m_heightMeters = 0.0;
	double m_westDegrees = 0.0;
	double m_southDegrees = 0.0;
	double m_eastDegrees = 0.0;
	double m_northDegrees = 0.0;
	bool m_isPoint = true;
};

/** Immutable, lifetime-safe result of one geocoder request. */
class CesiumGeocoderResult : public Resource {
	GDCLASS(CesiumGeocoderResult, Resource)

public:
	Array get_attributions() const;
	Array get_features() const;
	void initialize(const Array& attributions, const Array& features);

protected:
	static void _bind_methods();

private:
	Array m_attributions;
	Array m_features;
};

class CesiumGeocoderService;

/** Ref-counted handle for an asynchronous geocoder request. */
class CesiumGeocoderRequest : public RefCounted {
	GDCLASS(CesiumGeocoderRequest, RefCounted)

public:
	enum Status {
		Pending = 0,
		Completed = 1,
		Failed = 2,
		Cancelled = 3,
	};

	int64_t get_request_id() const;
	int32_t get_status() const;
	bool is_finished() const;
	bool is_cancelled() const;
	int32_t get_provider() const;
	int32_t get_request_type() const;
	const String& get_query() const;
	Ref<CesiumGeocoderResult> get_result() const;
	int32_t get_http_status_code() const;
	const String& get_error_code() const;
	const String& get_error_message() const;
	void cancel();

	void initialize(
		const ObjectID& service,
		int64_t requestId,
		int32_t provider,
		int32_t requestType,
		const String& query
	);
	void complete(const Ref<CesiumGeocoderResult>& result, int32_t httpStatusCode);
	void fail(
		const String& errorCode,
		const String& errorMessage,
		int32_t httpStatusCode
	);
	void cancel_from_service();

protected:
	static void _bind_methods();

private:
	ObjectID m_service;
	int64_t m_requestId = 0;
	Status m_status = Pending;
	int32_t m_provider = 2;
	int32_t m_requestType = 0;
	String m_query;
	Ref<CesiumGeocoderResult> m_result;
	int32_t m_httpStatusCode = 0;
	String m_errorCode;
	String m_errorMessage;
};

VARIANT_ENUM_CAST(CesiumGeocoderRequest::Status);

/**
 * Asynchronous Cesium ion geocoder service.
 *
 * This standalone Node owns a small Native async/network environment, caches
 * ion application data per API endpoint, and returns copied Godot Resources.
 * It does not require a tileset and never includes the access token in its
 * diagnostics or result objects.
 */
class CesiumGeocoderService : public Node {
	GDCLASS(CesiumGeocoderService, Node)

public:
	enum Provider {
		Google = 0,
		Bing = 1,
		Default = 2,
	};
	enum RequestType {
		Search = 0,
		Autocomplete = 1,
	};

	CesiumGeocoderService();
	~CesiumGeocoderService() override;

	void _enter_tree() override;
	void _exit_tree() override;
	void _process(double delta) override;

	void set_access_token(const String& value);
	const String& get_access_token() const;
	void set_api_url(const String& value);
	const String& get_api_url() const;
	void set_worker_thread_count(int32_t value);
	int32_t get_worker_thread_count() const;
	void set_maximum_network_retries(int32_t value);
	int32_t get_maximum_network_retries() const;
	void set_network_retry_initial_delay_seconds(double value);
	double get_network_retry_initial_delay_seconds() const;
	void set_network_retry_maximum_delay_seconds(double value);
	double get_network_retry_maximum_delay_seconds() const;

	Ref<CesiumGeocoderRequest> geocode(
		const String& query,
		int32_t provider,
		int32_t requestType
	);
	Ref<CesiumGeocoderRequest> search(const String& query, int32_t provider);
	Ref<CesiumGeocoderRequest> autocomplete(
		const String& query,
		int32_t provider
	);
	void cancel_request(int64_t requestId);
	void cancel_all();
	int32_t get_pending_request_count() const;
	Dictionary get_configuration() const;

protected:
	static void _bind_methods();

private:
	struct SubmittedRequest {
		Ref<CesiumGeocoderRequest> request;
		String accessToken;
		String query;
		int32_t provider = Default;
		int32_t requestType = Search;
	};
	struct Runtime;
	void ensure_runtime();
	void shutdown_runtime();
	void submit_request(SubmittedRequest&& submitted);
	void cancel_runtime_requests();
	void finish_runtime_recreation();
	void start_application_data_request();
	void start_geocode_request(
		int64_t requestId,
		const String& accessToken,
		const String& query,
		int32_t provider,
		int32_t requestType
	);
	void handle_application_data_response(
		uint64_t generation,
		CesiumIonClient::Response<CesiumIonClient::ApplicationData>&& response
	);
	void handle_geocode_response(
		int64_t requestId,
		CesiumIonClient::Response<CesiumIonClient::GeocoderResult>&& response
	);
	void handle_request_exception(int64_t requestId, const String& message);
	void fail_waiting_requests(
		const String& errorCode,
		const String& errorMessage,
		int32_t httpStatusCode
	);
	void prune_continuations();
	void recreate_runtime_if_idle();
	String normalized_api_url(const String& value) const;

	String m_accessToken;
	String m_apiUrl = "https://api.cesium.com/";
	int32_t m_workerThreadCount = 1;
	int32_t m_maximumNetworkRetries = 3;
	double m_networkRetryInitialDelaySeconds = 0.25;
	double m_networkRetryMaximumDelaySeconds = 4.0;
	int64_t m_nextRequestId = 1;
	bool m_runtimeRecreationPending = false;
	bool m_shutdownInProgress = false;
	std::vector<SubmittedRequest> m_deferredRequests;
	std::unique_ptr<Runtime> m_runtime;
};

VARIANT_ENUM_CAST(CesiumGeocoderService::Provider);
VARIANT_ENUM_CAST(CesiumGeocoderService::RequestType);

#endif // CESIUM_GEOCODER_SERVICE_H
