// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumWebMapServiceRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumWebMapServiceRasterOverlay.h"

#include <CesiumRasterOverlays/WebMapServiceRasterOverlay.h>

#include <algorithm>

#define CESIUM_WMS_STRING_SETTER(method, field) \
	void CesiumWebMapServiceRasterOverlay::method(const String& value) { \
		if (this->field == value) return; \
		this->field = value; \
		this->refresh(); \
	}

CESIUM_WMS_STRING_SETTER(set_base_url, m_baseUrl)
const String& CesiumWebMapServiceRasterOverlay::get_base_url() const { return this->m_baseUrl; }
CESIUM_WMS_STRING_SETTER(set_layers, m_layers)
const String& CesiumWebMapServiceRasterOverlay::get_layers() const { return this->m_layers; }
CESIUM_WMS_STRING_SETTER(set_version, m_version)
const String& CesiumWebMapServiceRasterOverlay::get_version() const { return this->m_version; }
CESIUM_WMS_STRING_SETTER(set_format, m_format)
const String& CesiumWebMapServiceRasterOverlay::get_format() const { return this->m_format; }

void CesiumWebMapServiceRasterOverlay::set_tile_width(int32_t value) {
	const int32_t bounded = std::clamp(value, 64, 2048);
	if (this->m_tileWidth == bounded) return;
	this->m_tileWidth = bounded;
	this->refresh();
}
int32_t CesiumWebMapServiceRasterOverlay::get_tile_width() const { return this->m_tileWidth; }

void CesiumWebMapServiceRasterOverlay::set_tile_height(int32_t value) {
	const int32_t bounded = std::clamp(value, 64, 2048);
	if (this->m_tileHeight == bounded) return;
	this->m_tileHeight = bounded;
	this->refresh();
}
int32_t CesiumWebMapServiceRasterOverlay::get_tile_height() const { return this->m_tileHeight; }

void CesiumWebMapServiceRasterOverlay::set_minimum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_minimumLevel == bounded) return;
	this->m_minimumLevel = bounded;
	this->refresh();
}
int32_t CesiumWebMapServiceRasterOverlay::get_minimum_level() const { return this->m_minimumLevel; }

void CesiumWebMapServiceRasterOverlay::set_maximum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_maximumLevel == bounded) return;
	this->m_maximumLevel = bounded;
	this->refresh();
}
int32_t CesiumWebMapServiceRasterOverlay::get_maximum_level() const { return this->m_maximumLevel; }

void CesiumWebMapServiceRasterOverlay::set_request_headers(const Dictionary& value) {
	this->m_requestHeaders = value.duplicate(true);
	this->refresh();
}
Dictionary CesiumWebMapServiceRasterOverlay::get_request_headers() const {
	return this->m_requestHeaders.duplicate(true);
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumWebMapServiceRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_baseUrl.is_empty()) return nullptr;

	CesiumRasterOverlays::WebMapServiceRasterOverlayOptions providerOptions;
	providerOptions.version = this->m_version.utf8().get_data();
	providerOptions.layers = this->m_layers.utf8().get_data();
	providerOptions.format = this->m_format.utf8().get_data();
	providerOptions.tileWidth = this->m_tileWidth;
	providerOptions.tileHeight = this->m_tileHeight;
	providerOptions.minimumLevel = this->m_minimumLevel;
	providerOptions.maximumLevel = std::max(this->m_minimumLevel, this->m_maximumLevel);

	return std::make_unique<CesiumRasterOverlays::WebMapServiceRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		this->m_baseUrl.utf8().get_data(),
		this->request_headers_from_dictionary(this->m_requestHeaders),
		providerOptions,
		options
	);
}

String CesiumWebMapServiceRasterOverlay::provider_type() const { return "web_map_service"; }

void CesiumWebMapServiceRasterOverlay::append_provider_configuration(Dictionary& result) const {
	result["base_url"] = this->m_baseUrl;
	result["layers"] = this->m_layers;
	result["version"] = this->m_version;
	result["format"] = this->m_format;
	result["tile_width"] = this->m_tileWidth;
	result["tile_height"] = this->m_tileHeight;
	result["minimum_level"] = this->m_minimumLevel;
	result["maximum_level"] = std::max(this->m_minimumLevel, this->m_maximumLevel);
	result["request_headers"] = this->m_requestHeaders.duplicate(true);
}

#undef CESIUM_WMS_STRING_SETTER

void CesiumWebMapServiceRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_url", "value"), &CesiumWebMapServiceRasterOverlay::set_base_url);
	ClassDB::bind_method(D_METHOD("get_base_url"), &CesiumWebMapServiceRasterOverlay::get_base_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_url"), "set_base_url", "get_base_url");
	ClassDB::bind_method(D_METHOD("set_layers", "value"), &CesiumWebMapServiceRasterOverlay::set_layers);
	ClassDB::bind_method(D_METHOD("get_layers"), &CesiumWebMapServiceRasterOverlay::get_layers);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "layers"), "set_layers", "get_layers");
	ClassDB::bind_method(D_METHOD("set_version", "value"), &CesiumWebMapServiceRasterOverlay::set_version);
	ClassDB::bind_method(D_METHOD("get_version"), &CesiumWebMapServiceRasterOverlay::get_version);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "version", PROPERTY_HINT_ENUM, "1.3.0,1.1.1"), "set_version", "get_version");
	ClassDB::bind_method(D_METHOD("set_format", "value"), &CesiumWebMapServiceRasterOverlay::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &CesiumWebMapServiceRasterOverlay::get_format);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "format", PROPERTY_HINT_ENUM, "image/png,image/jpeg"), "set_format", "get_format");
	ClassDB::bind_method(D_METHOD("set_tile_width", "value"), &CesiumWebMapServiceRasterOverlay::set_tile_width);
	ClassDB::bind_method(D_METHOD("get_tile_width"), &CesiumWebMapServiceRasterOverlay::get_tile_width);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_width", PROPERTY_HINT_RANGE, "64,2048,1"), "set_tile_width", "get_tile_width");
	ClassDB::bind_method(D_METHOD("set_tile_height", "value"), &CesiumWebMapServiceRasterOverlay::set_tile_height);
	ClassDB::bind_method(D_METHOD("get_tile_height"), &CesiumWebMapServiceRasterOverlay::get_tile_height);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_height", PROPERTY_HINT_RANGE, "64,2048,1"), "set_tile_height", "get_tile_height");
	ClassDB::bind_method(D_METHOD("set_minimum_level", "value"), &CesiumWebMapServiceRasterOverlay::set_minimum_level);
	ClassDB::bind_method(D_METHOD("get_minimum_level"), &CesiumWebMapServiceRasterOverlay::get_minimum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_minimum_level", "get_minimum_level");
	ClassDB::bind_method(D_METHOD("set_maximum_level", "value"), &CesiumWebMapServiceRasterOverlay::set_maximum_level);
	ClassDB::bind_method(D_METHOD("get_maximum_level"), &CesiumWebMapServiceRasterOverlay::get_maximum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_maximum_level", "get_maximum_level");
	ClassDB::bind_method(D_METHOD("set_request_headers", "value"), &CesiumWebMapServiceRasterOverlay::set_request_headers);
	ClassDB::bind_method(D_METHOD("get_request_headers"), &CesiumWebMapServiceRasterOverlay::get_request_headers);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "request_headers"), "set_request_headers", "get_request_headers");
}
