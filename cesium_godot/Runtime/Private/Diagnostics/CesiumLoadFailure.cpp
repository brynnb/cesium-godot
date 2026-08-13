// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"

Object* CesiumLoadFailure::get_source() const {
	return this->m_sourceInstanceId == 0
		? nullptr
		: ObjectDB::get_instance(this->m_sourceInstanceId);
}

int64_t CesiumLoadFailure::get_source_instance_id() const {
	return static_cast<int64_t>(this->m_sourceInstanceId);
}

int32_t CesiumLoadFailure::get_category() const {
	return static_cast<int32_t>(this->m_category);
}

String CesiumLoadFailure::get_category_name() const {
	switch (this->m_category) {
	case Category::Tileset: return "tileset";
	case Category::TileContent: return "tile_content";
	case Category::RasterOverlay: return "raster_overlay";
	case Category::Network: return "network";
	case Category::Decode: return "decode";
	case Category::Material: return "material";
	case Category::Renderer: return "renderer";
	case Category::Cache: return "cache";
	case Category::Unknown:
	default: return "unknown";
	}
}

int32_t CesiumLoadFailure::get_stage() const {
	return static_cast<int32_t>(this->m_stage);
}

String CesiumLoadFailure::get_stage_name() const {
	switch (this->m_stage) {
	case Stage::IonEndpoint: return "ion_endpoint";
	case Stage::TilesetJson: return "tileset_json";
	case Stage::TileContentRequest: return "tile_content_request";
	case Stage::RasterTileProvider: return "raster_tile_provider";
	case Stage::RasterTileRequest: return "raster_tile_request";
	case Stage::NetworkRequest: return "network_request";
	case Stage::GltfDecode: return "gltf_decode";
	case Stage::MaterialCreation: return "material_creation";
	case Stage::RendererPreparation: return "renderer_preparation";
	case Stage::TextureUpload: return "texture_upload";
	case Stage::CacheOpen: return "cache_open";
	case Stage::CacheMaintenance: return "cache_maintenance";
	case Stage::StageUnknown:
	default: return "unknown";
	}
}

const String& CesiumLoadFailure::get_message() const { return this->m_message; }
const String& CesiumLoadFailure::get_url() const { return this->m_url; }
const String& CesiumLoadFailure::get_tile_id() const { return this->m_tileId; }
const String& CesiumLoadFailure::get_overlay_key() const { return this->m_overlayKey; }
int32_t CesiumLoadFailure::get_http_status_code() const { return this->m_httpStatusCode; }
bool CesiumLoadFailure::get_terminal() const { return this->m_terminal; }
bool CesiumLoadFailure::get_retryable() const { return this->m_retryable; }
bool CesiumLoadFailure::get_retry_scheduled() const { return this->m_retryScheduled; }
int32_t CesiumLoadFailure::get_attempt() const { return this->m_attempt; }
int32_t CesiumLoadFailure::get_maximum_attempts() const { return this->m_maximumAttempts; }
double CesiumLoadFailure::get_retry_delay_seconds() const { return this->m_retryDelaySeconds; }

Dictionary CesiumLoadFailure::to_dictionary() const {
	Dictionary result;
	result["source"] = this->get_source();
	result["source_instance_id"] = this->get_source_instance_id();
	result["category"] = this->get_category();
	result["category_name"] = this->get_category_name();
	result["stage"] = this->get_stage();
	result["stage_name"] = this->get_stage_name();
	result["message"] = this->m_message;
	result["url"] = this->m_url;
	result["tile_id"] = this->m_tileId;
	result["overlay_key"] = this->m_overlayKey;
	result["http_status_code"] = this->m_httpStatusCode;
	result["terminal"] = this->m_terminal;
	result["retryable"] = this->m_retryable;
	result["retry_scheduled"] = this->m_retryScheduled;
	result["attempt"] = this->m_attempt;
	result["maximum_attempts"] = this->m_maximumAttempts;
	result["retry_delay_seconds"] = this->m_retryDelaySeconds;
	return result;
}

void CesiumLoadFailure::initialize(
	uint64_t sourceInstanceId,
	Category category,
	Stage stage,
	const String& message,
	const String& url,
	const String& tileId,
	const String& overlayKey,
	int32_t httpStatusCode,
	bool terminal,
	bool retryable,
	bool retryScheduled,
	int32_t attempt,
	int32_t maximumAttempts,
	double retryDelaySeconds
) {
	this->m_sourceInstanceId = sourceInstanceId;
	this->m_category = category;
	this->m_stage = stage;
	this->m_message = message;
	this->m_url = url;
	this->m_tileId = tileId;
	this->m_overlayKey = overlayKey;
	this->m_httpStatusCode = httpStatusCode;
	this->m_terminal = terminal;
	this->m_retryable = retryable;
	this->m_retryScheduled = retryScheduled;
	this->m_attempt = attempt;
	this->m_maximumAttempts = maximumAttempts;
	this->m_retryDelaySeconds = retryDelaySeconds;
}

void CesiumLoadFailure::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_source"), &CesiumLoadFailure::get_source);
	ClassDB::bind_method(D_METHOD("get_source_instance_id"), &CesiumLoadFailure::get_source_instance_id);
	ClassDB::bind_method(D_METHOD("get_category"), &CesiumLoadFailure::get_category);
	ClassDB::bind_method(D_METHOD("get_category_name"), &CesiumLoadFailure::get_category_name);
	ClassDB::bind_method(D_METHOD("get_stage"), &CesiumLoadFailure::get_stage);
	ClassDB::bind_method(D_METHOD("get_stage_name"), &CesiumLoadFailure::get_stage_name);
	ClassDB::bind_method(D_METHOD("get_message"), &CesiumLoadFailure::get_message);
	ClassDB::bind_method(D_METHOD("get_url"), &CesiumLoadFailure::get_url);
	ClassDB::bind_method(D_METHOD("get_tile_id"), &CesiumLoadFailure::get_tile_id);
	ClassDB::bind_method(D_METHOD("get_overlay_key"), &CesiumLoadFailure::get_overlay_key);
	ClassDB::bind_method(D_METHOD("get_http_status_code"), &CesiumLoadFailure::get_http_status_code);
	ClassDB::bind_method(D_METHOD("get_terminal"), &CesiumLoadFailure::get_terminal);
	ClassDB::bind_method(D_METHOD("get_retryable"), &CesiumLoadFailure::get_retryable);
	ClassDB::bind_method(D_METHOD("get_retry_scheduled"), &CesiumLoadFailure::get_retry_scheduled);
	ClassDB::bind_method(D_METHOD("get_attempt"), &CesiumLoadFailure::get_attempt);
	ClassDB::bind_method(D_METHOD("get_maximum_attempts"), &CesiumLoadFailure::get_maximum_attempts);
	ClassDB::bind_method(D_METHOD("get_retry_delay_seconds"), &CesiumLoadFailure::get_retry_delay_seconds);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &CesiumLoadFailure::to_dictionary);

	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_UNKNOWN", Category::Unknown);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_TILESET", Category::Tileset);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_TILE_CONTENT", Category::TileContent);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_RASTER_OVERLAY", Category::RasterOverlay);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_NETWORK", Category::Network);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_DECODE", Category::Decode);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_MATERIAL", Category::Material);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_RENDERER", Category::Renderer);
	ClassDB::bind_integer_constant(get_class_static(), "Category", "CATEGORY_CACHE", Category::Cache);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_UNKNOWN", Stage::StageUnknown);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_ION_ENDPOINT", Stage::IonEndpoint);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_TILESET_JSON", Stage::TilesetJson);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_TILE_CONTENT_REQUEST", Stage::TileContentRequest);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_RASTER_TILE_PROVIDER", Stage::RasterTileProvider);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_RASTER_TILE_REQUEST", Stage::RasterTileRequest);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_NETWORK_REQUEST", Stage::NetworkRequest);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_GLTF_DECODE", Stage::GltfDecode);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_MATERIAL_CREATION", Stage::MaterialCreation);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_RENDERER_PREPARATION", Stage::RendererPreparation);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_TEXTURE_UPLOAD", Stage::TextureUpload);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_CACHE_OPEN", Stage::CacheOpen);
	ClassDB::bind_integer_constant(get_class_static(), "Stage", "STAGE_CACHE_MAINTENANCE", Stage::CacheMaintenance);

#define CESIUM_READ_ONLY_PROPERTY(type, name) \
	ADD_PROPERTY(PropertyInfo(type, #name, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_" #name)
	CESIUM_READ_ONLY_PROPERTY(Variant::OBJECT, source);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, source_instance_id);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, category);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, category_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, stage);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, stage_name);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, message);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, url);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, tile_id);
	CESIUM_READ_ONLY_PROPERTY(Variant::STRING, overlay_key);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, http_status_code);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, terminal);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, retryable);
	CESIUM_READ_ONLY_PROPERTY(Variant::BOOL, retry_scheduled);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, attempt);
	CESIUM_READ_ONLY_PROPERTY(Variant::INT, maximum_attempts);
	CESIUM_READ_ONLY_PROPERTY(Variant::FLOAT, retry_delay_seconds);
#undef CESIUM_READ_ONLY_PROPERTY
}
