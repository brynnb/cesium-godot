// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumGeoJsonDocumentRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumGeoJsonDocumentRasterOverlay.h"

#include "Models/CesiumGDConfig.h"
#include "Runtime/Public/Vector/CesiumGeoJsonDocument.h"
#include "Runtime/Public/Vector/CesiumVectorStyle.h"
#include "Runtime/Public/Networking/CesiumUrlUtility.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumVectorOverlays/GeoJsonDocumentRasterOverlay.h>
#include <CesiumUtility/ErrorList.h>
#include <CesiumUtility/Result.h>
#include <CesiumVectorData/GeoJsonDocument.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <span>
#include <string>
#include <utility>

namespace {
using DocumentResult = CesiumUtility::Result<CesiumVectorData::GeoJsonDocument>;

PackedStringArray to_strings(const std::vector<std::string>& source) {
	PackedStringArray result;
	result.resize(static_cast<int64_t>(source.size()));
	for (size_t index = 0; index < source.size(); ++index) {
		result.set(static_cast<int64_t>(index), String::utf8(source[index].c_str()));
	}
	return result;
}

DocumentResult rejected_document(std::exception&& exception) {
	return DocumentResult(CesiumUtility::ErrorList::error(exception.what()));
}
} // namespace

CesiumGeoJsonDocumentRasterOverlay::CesiumGeoJsonDocumentRasterOverlay() {
	this->m_defaultStyle.instantiate();
	this->connect_style();
}

CesiumGeoJsonDocumentRasterOverlay::~CesiumGeoJsonDocumentRasterOverlay() {
	this->disconnect_document();
	this->disconnect_style();
}

void CesiumGeoJsonDocumentRasterOverlay::set_source(int32_t source) {
	const Source bounded = source == FromString ? FromString
		: source == FromUrl ? FromUrl
		: source == FromCesiumIon ? FromCesiumIon
		: FromDocument;
	if (this->m_source == bounded) return;
	this->m_source = bounded;
	this->notify_property_list_changed();
	this->refresh();
}
int32_t CesiumGeoJsonDocumentRasterOverlay::get_source() const { return this->m_source; }

void CesiumGeoJsonDocumentRasterOverlay::set_document(
	const Ref<CesiumGeoJsonDocument>& document
) {
	if (this->m_document == document) return;
	this->disconnect_document();
	this->m_document = document;
	this->connect_document();
	this->refresh();
}
Ref<CesiumGeoJsonDocument> CesiumGeoJsonDocumentRasterOverlay::get_document() const { return this->m_document; }

void CesiumGeoJsonDocumentRasterOverlay::set_geo_json(const String& geoJson) {
	if (this->m_geoJson == geoJson) return;
	this->m_geoJson = geoJson;
	this->refresh();
}
const String& CesiumGeoJsonDocumentRasterOverlay::get_geo_json() const { return this->m_geoJson; }

void CesiumGeoJsonDocumentRasterOverlay::set_url(const String& url) {
	if (this->m_url == url) return;
	this->m_url = url;
	this->refresh();
}
const String& CesiumGeoJsonDocumentRasterOverlay::get_url() const { return this->m_url; }

void CesiumGeoJsonDocumentRasterOverlay::set_request_headers(const Dictionary& headers) {
	this->m_requestHeaders = headers.duplicate(true);
	this->refresh();
}
Dictionary CesiumGeoJsonDocumentRasterOverlay::get_request_headers() const { return this->m_requestHeaders.duplicate(true); }

void CesiumGeoJsonDocumentRasterOverlay::set_ion_asset_id(int64_t assetId) {
	const int64_t bounded = std::max<int64_t>(0, assetId);
	if (this->m_ionAssetId == bounded) return;
	this->m_ionAssetId = bounded;
	this->refresh();
}
int64_t CesiumGeoJsonDocumentRasterOverlay::get_ion_asset_id() const { return this->m_ionAssetId; }

void CesiumGeoJsonDocumentRasterOverlay::set_ion_access_token(const String& token) {
	if (this->m_ionAccessToken == token) return;
	this->m_ionAccessToken = token;
	this->refresh();
}
const String& CesiumGeoJsonDocumentRasterOverlay::get_ion_access_token() const { return this->m_ionAccessToken; }

void CesiumGeoJsonDocumentRasterOverlay::set_ion_api_url(const String& url) {
	if (this->m_ionApiUrl == url) return;
	this->m_ionApiUrl = url;
	this->refresh();
}
const String& CesiumGeoJsonDocumentRasterOverlay::get_ion_api_url() const { return this->m_ionApiUrl; }

void CesiumGeoJsonDocumentRasterOverlay::set_mip_levels(int32_t levels) {
	const int32_t bounded = std::clamp(levels, 0, 8);
	if (this->m_mipLevels == bounded) return;
	this->m_mipLevels = bounded;
	this->refresh();
}
int32_t CesiumGeoJsonDocumentRasterOverlay::get_mip_levels() const { return this->m_mipLevels; }

void CesiumGeoJsonDocumentRasterOverlay::set_default_style(
	const Ref<CesiumVectorStyle>& style
) {
	if (this->m_defaultStyle == style) return;
	this->disconnect_style();
	this->m_defaultStyle = style;
	if (this->m_defaultStyle.is_null()) this->m_defaultStyle.instantiate();
	this->connect_style();
	this->refresh();
}
Ref<CesiumVectorStyle> CesiumGeoJsonDocumentRasterOverlay::get_default_style() const { return this->m_defaultStyle; }
PackedStringArray CesiumGeoJsonDocumentRasterOverlay::get_last_document_errors() const { return this->m_lastDocumentErrors; }
PackedStringArray CesiumGeoJsonDocumentRasterOverlay::get_last_document_warnings() const { return this->m_lastDocumentWarnings; }

void CesiumGeoJsonDocumentRasterOverlay::_on_document_changed() { this->refresh(); }
void CesiumGeoJsonDocumentRasterOverlay::_on_default_style_changed() { this->refresh(); }

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumGeoJsonDocumentRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	const CesiumAsync::AsyncSystem* asyncSystem = this->get_async_system();
	if (asyncSystem == nullptr) return nullptr;

	CesiumVectorOverlays::GeoJsonDocumentRasterOverlayOptions vectorOptions{
		this->m_defaultStyle.is_valid()
			? this->m_defaultStyle->to_native()
			: CesiumVectorData::VectorStyle(),
		options.ellipsoid,
		static_cast<uint32_t>(this->m_mipLevels)
	};
	const std::string name = this->get_material_key().utf8().get_data();
	const uint64_t generation = ++this->m_documentGeneration;
	this->m_lastDocumentErrors.clear();
	this->m_lastDocumentWarnings.clear();

	if (this->m_source == FromDocument) {
		if (this->m_document.is_null() || !this->m_document->is_valid()) return nullptr;
		const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document =
			this->m_document->get_native_document();
		this->handle_document_result(
			generation,
			document,
			this->m_document->get_errors(),
			this->m_document->get_warnings()
		);
		return std::make_unique<CesiumVectorOverlays::GeoJsonDocumentRasterOverlay>(
			*asyncSystem,
			name,
			document,
			vectorOptions,
			options
		);
	}

	const ObjectID objectId(this->get_instance_id());
	auto finish = [objectId, generation](DocumentResult&& result) {
		const PackedStringArray errors = to_strings(result.errors.errors);
		const PackedStringArray warnings = to_strings(result.errors.warnings);
		std::shared_ptr<CesiumVectorData::GeoJsonDocument> document = result.value
			? std::make_shared<CesiumVectorData::GeoJsonDocument>(
				std::move(*result.value)
			)
			: std::shared_ptr<CesiumVectorData::GeoJsonDocument>();
		CesiumGeoJsonDocumentRasterOverlay* owner = Object::cast_to<
			CesiumGeoJsonDocumentRasterOverlay
		>(ObjectDB::get_instance(objectId));
		if (owner != nullptr) {
			owner->handle_document_result(
				generation,
				document,
				errors,
				warnings
			);
		}
		return document;
	};

	if (this->m_source == FromString) {
		if (this->m_geoJson.is_empty()) return nullptr;
		const std::string source = this->m_geoJson.utf8().get_data();
		auto future = asyncSystem->runInWorkerThread([source]() {
			return CesiumVectorData::GeoJsonDocument::fromGeoJson(
				std::span<const std::byte>(
					reinterpret_cast<const std::byte*>(source.data()),
					source.size()
				)
			);
		}).catchImmediately(rejected_document).thenInMainThread(finish);
		return std::make_unique<CesiumVectorOverlays::GeoJsonDocumentRasterOverlay>(
			name,
			std::move(future),
			vectorOptions,
			options
		);
	}

	const std::shared_ptr<CesiumAsync::IAssetAccessor> accessor =
		this->get_asset_accessor();
	if (accessor == nullptr) return nullptr;
	if (this->m_source == FromUrl) {
		const String url = this->resolved_url();
		if (url.is_empty()) return nullptr;
		auto future = CesiumVectorData::GeoJsonDocument::fromUrl(
			*asyncSystem,
			accessor,
			url.utf8().get_data(),
			this->request_headers_from_dictionary(this->m_requestHeaders)
		).catchImmediately(rejected_document).thenInMainThread(finish);
		return std::make_unique<CesiumVectorOverlays::GeoJsonDocumentRasterOverlay>(
			name,
			std::move(future),
			vectorOptions,
			options
		);
	}

	if (this->m_ionAssetId <= 0 || this->m_ionApiUrl.is_empty()) return nullptr;
	String apiUrl = this->m_ionApiUrl;
	if (!apiUrl.ends_with("/")) apiUrl += "/";
	const String token = this->m_ionAccessToken.is_empty()
		? CesiumGDConfig::get_singleton(this)->get_access_token()
		: this->m_ionAccessToken;
	auto future = CesiumVectorData::GeoJsonDocument::fromCesiumIonAsset(
		*asyncSystem,
		accessor,
		this->m_ionAssetId,
		token.utf8().get_data(),
		apiUrl.utf8().get_data()
	).catchImmediately(rejected_document).thenInMainThread(finish);
	return std::make_unique<CesiumVectorOverlays::GeoJsonDocumentRasterOverlay>(
		name,
		std::move(future),
		vectorOptions,
		options
	);
}

void CesiumGeoJsonDocumentRasterOverlay::on_removing_from_tileset(
	Cesium3DTileset* tileset,
	CesiumRasterOverlays::RasterOverlay* overlay
) {
	(void)tileset;
	(void)overlay;
	// Async parsing and requests cannot retain this Godot node. Advancing the
	// generation also prevents a manually removed provider from publishing a
	// late success or failure signal.
	++this->m_documentGeneration;
}

String CesiumGeoJsonDocumentRasterOverlay::provider_type() const { return "geo_json"; }

void CesiumGeoJsonDocumentRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["source"] = static_cast<int32_t>(this->m_source);
	result["document_valid"] = this->m_document.is_valid() && this->m_document->is_valid();
	result["inline_bytes"] = this->m_geoJson.utf8().length();
	result["url"] = this->m_url;
	result["request_headers"] = this->m_requestHeaders.duplicate(true);
	result["ion_asset_id"] = this->m_ionAssetId;
	result["ion_api_url"] = this->m_ionApiUrl;
	result["ion_access_token_source"] = this->m_ionAccessToken.is_empty()
		? "project_default" : "overlay_override";
	result["mip_levels"] = this->m_mipLevels;
	result["last_document_errors"] = this->m_lastDocumentErrors;
	result["last_document_warnings"] = this->m_lastDocumentWarnings;
}

void CesiumGeoJsonDocumentRasterOverlay::disconnect_document() {
	const Callable callback(this, "_on_document_changed");
	if (this->m_document.is_valid() && this->m_document->is_connected("changed", callback)) {
		this->m_document->disconnect("changed", callback);
	}
}

void CesiumGeoJsonDocumentRasterOverlay::connect_document() {
	const Callable callback(this, "_on_document_changed");
	if (this->m_document.is_valid() && !this->m_document->is_connected("changed", callback)) {
		this->m_document->connect("changed", callback);
	}
}

void CesiumGeoJsonDocumentRasterOverlay::disconnect_style() {
	const Callable callback(this, "_on_default_style_changed");
	if (this->m_defaultStyle.is_valid() && this->m_defaultStyle->is_connected("changed", callback)) {
		this->m_defaultStyle->disconnect("changed", callback);
	}
}

void CesiumGeoJsonDocumentRasterOverlay::connect_style() {
	const Callable callback(this, "_on_default_style_changed");
	if (this->m_defaultStyle.is_valid() && !this->m_defaultStyle->is_connected("changed", callback)) {
		this->m_defaultStyle->connect("changed", callback);
	}
}

void CesiumGeoJsonDocumentRasterOverlay::handle_document_result(
	uint64_t generation,
	const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document,
	const PackedStringArray& errors,
	const PackedStringArray& warnings
) {
	if (generation != this->m_documentGeneration) return;
	this->m_lastDocumentErrors = errors;
	this->m_lastDocumentWarnings = warnings;
	if (document == nullptr) {
		this->emit_signal("document_load_failed", errors, warnings);
		return;
	}
	Ref<CesiumGeoJsonDocument> godotDocument;
	godotDocument.instantiate();
	godotDocument->initialize_native(
		std::shared_ptr<CesiumVectorData::GeoJsonDocument>(document),
		errors,
		warnings
	);
	this->emit_signal("document_loaded", godotDocument);
}

String CesiumGeoJsonDocumentRasterOverlay::resolved_url() const {
	if (this->m_url.begins_with("res://") || this->m_url.begins_with("user://")) {
		return CesiumUrlUtility::local_path_to_file_url(this->m_url);
	}
	return this->m_url;
}

void CesiumGeoJsonDocumentRasterOverlay::_get_property_list(
	List<PropertyInfo>* properties
) const {
#if defined(CESIUM_GD_MODULE)
	for (int32_t index = 0; index < properties->size(); ++index) {
		PropertyInfo& property = properties->get(index);
#elif defined(CESIUM_GD_EXT)
	for (auto iterator = properties->begin(); iterator != properties->end(); ++iterator) {
		PropertyInfo& property = *iterator;
#endif
		const StringName name = property.name;
		if (name == StringName("document")) {
			property.usage = this->m_source == FromDocument ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_READ_ONLY;
		} else if (name == StringName("geo_json")) {
			property.usage = this->m_source == FromString ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_READ_ONLY;
		} else if (name == StringName("url") || name == StringName("request_headers")) {
			property.usage = this->m_source == FromUrl ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_READ_ONLY;
		} else if (name == StringName("ion_asset_id") || name == StringName("ion_access_token") || name == StringName("ion_api_url")) {
			property.usage = this->m_source == FromCesiumIon ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_READ_ONLY;
		}
	}
}

void CesiumGeoJsonDocumentRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source", "source"), &CesiumGeoJsonDocumentRasterOverlay::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &CesiumGeoJsonDocumentRasterOverlay::get_source);
	ClassDB::bind_method(D_METHOD("set_document", "document"), &CesiumGeoJsonDocumentRasterOverlay::set_document);
	ClassDB::bind_method(D_METHOD("get_document"), &CesiumGeoJsonDocumentRasterOverlay::get_document);
	ClassDB::bind_method(D_METHOD("set_geo_json", "geo_json"), &CesiumGeoJsonDocumentRasterOverlay::set_geo_json);
	ClassDB::bind_method(D_METHOD("get_geo_json"), &CesiumGeoJsonDocumentRasterOverlay::get_geo_json);
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CesiumGeoJsonDocumentRasterOverlay::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CesiumGeoJsonDocumentRasterOverlay::get_url);
	ClassDB::bind_method(D_METHOD("set_request_headers", "headers"), &CesiumGeoJsonDocumentRasterOverlay::set_request_headers);
	ClassDB::bind_method(D_METHOD("get_request_headers"), &CesiumGeoJsonDocumentRasterOverlay::get_request_headers);
	ClassDB::bind_method(D_METHOD("set_ion_asset_id", "asset_id"), &CesiumGeoJsonDocumentRasterOverlay::set_ion_asset_id);
	ClassDB::bind_method(D_METHOD("get_ion_asset_id"), &CesiumGeoJsonDocumentRasterOverlay::get_ion_asset_id);
	ClassDB::bind_method(D_METHOD("set_ion_access_token", "token"), &CesiumGeoJsonDocumentRasterOverlay::set_ion_access_token);
	ClassDB::bind_method(D_METHOD("get_ion_access_token"), &CesiumGeoJsonDocumentRasterOverlay::get_ion_access_token);
	ClassDB::bind_method(D_METHOD("set_ion_api_url", "url"), &CesiumGeoJsonDocumentRasterOverlay::set_ion_api_url);
	ClassDB::bind_method(D_METHOD("get_ion_api_url"), &CesiumGeoJsonDocumentRasterOverlay::get_ion_api_url);
	ClassDB::bind_method(D_METHOD("set_mip_levels", "levels"), &CesiumGeoJsonDocumentRasterOverlay::set_mip_levels);
	ClassDB::bind_method(D_METHOD("get_mip_levels"), &CesiumGeoJsonDocumentRasterOverlay::get_mip_levels);
	ClassDB::bind_method(D_METHOD("set_default_style", "style"), &CesiumGeoJsonDocumentRasterOverlay::set_default_style);
	ClassDB::bind_method(D_METHOD("get_default_style"), &CesiumGeoJsonDocumentRasterOverlay::get_default_style);
	ClassDB::bind_method(D_METHOD("get_last_document_errors"), &CesiumGeoJsonDocumentRasterOverlay::get_last_document_errors);
	ClassDB::bind_method(D_METHOD("get_last_document_warnings"), &CesiumGeoJsonDocumentRasterOverlay::get_last_document_warnings);
	ClassDB::bind_method(D_METHOD("_on_document_changed"), &CesiumGeoJsonDocumentRasterOverlay::_on_document_changed);
	ClassDB::bind_method(D_METHOD("_on_default_style_changed"), &CesiumGeoJsonDocumentRasterOverlay::_on_default_style_changed);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "source", PROPERTY_HINT_ENUM, "Document,Inline String,URL or Local Path,Cesium ion"), "set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "CesiumGeoJsonDocument"), "set_document", "get_document");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "geo_json", PROPERTY_HINT_MULTILINE_TEXT), "set_geo_json", "get_geo_json");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url", PROPERTY_HINT_FILE, "*.json,*.geojson"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "request_headers"), "set_request_headers", "get_request_headers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ion_asset_id", PROPERTY_HINT_RANGE, "0,9223372036854775807,1"), "set_ion_asset_id", "get_ion_asset_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ion_access_token", PROPERTY_HINT_PASSWORD), "set_ion_access_token", "get_ion_access_token");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ion_api_url"), "set_ion_api_url", "get_ion_api_url");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mip_levels", PROPERTY_HINT_RANGE, "0,8,1"), "set_mip_levels", "get_mip_levels");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_style", PROPERTY_HINT_RESOURCE_TYPE, "CesiumVectorStyle"), "set_default_style", "get_default_style");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "last_document_errors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_last_document_errors");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "last_document_warnings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_last_document_warnings");
	ADD_SIGNAL(MethodInfo("document_loaded", PropertyInfo(Variant::OBJECT, "document", PROPERTY_HINT_RESOURCE_TYPE, "CesiumGeoJsonDocument")));
	ADD_SIGNAL(MethodInfo("document_load_failed", PropertyInfo(Variant::PACKED_STRING_ARRAY, "errors"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "warnings")));
	BIND_ENUM_CONSTANT(FromDocument);
	BIND_ENUM_CONSTANT(FromString);
	BIND_ENUM_CONSTANT(FromUrl);
	BIND_ENUM_CONSTANT(FromCesiumIon);
}
