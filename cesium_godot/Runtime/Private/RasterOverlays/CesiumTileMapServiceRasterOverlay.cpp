// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumTileMapServiceRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumTileMapServiceRasterOverlay.h"

#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>

#include <algorithm>

void CesiumTileMapServiceRasterOverlay::set_url(const String& url) {
	if (this->m_url == url) return;
	this->m_url = url;
	this->refresh();
}

const String& CesiumTileMapServiceRasterOverlay::get_url() const {
	return this->m_url;
}

void CesiumTileMapServiceRasterOverlay::set_specify_zoom_levels(bool value) {
	if (this->m_specifyZoomLevels == value) return;
	this->m_specifyZoomLevels = value;
	this->refresh();
}

bool CesiumTileMapServiceRasterOverlay::get_specify_zoom_levels() const {
	return this->m_specifyZoomLevels;
}

void CesiumTileMapServiceRasterOverlay::set_minimum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_minimumLevel == bounded) return;
	this->m_minimumLevel = bounded;
	this->refresh();
}

int32_t CesiumTileMapServiceRasterOverlay::get_minimum_level() const {
	return this->m_minimumLevel;
}

void CesiumTileMapServiceRasterOverlay::set_maximum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_maximumLevel == bounded) return;
	this->m_maximumLevel = bounded;
	this->refresh();
}

int32_t CesiumTileMapServiceRasterOverlay::get_maximum_level() const {
	return this->m_maximumLevel;
}

void CesiumTileMapServiceRasterOverlay::set_request_headers(
	const Dictionary& headers
) {
	this->m_requestHeaders = headers.duplicate(true);
	this->refresh();
}

Dictionary CesiumTileMapServiceRasterOverlay::get_request_headers() const {
	return this->m_requestHeaders.duplicate(true);
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumTileMapServiceRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_url.is_empty()) return nullptr;
	CesiumRasterOverlays::TileMapServiceRasterOverlayOptions tmsOptions;
	if (
		this->m_specifyZoomLevels &&
		this->m_maximumLevel > this->m_minimumLevel
	) {
		tmsOptions.minimumLevel = static_cast<uint32_t>(this->m_minimumLevel);
		tmsOptions.maximumLevel = static_cast<uint32_t>(this->m_maximumLevel);
	}
	return std::make_unique<
		CesiumRasterOverlays::TileMapServiceRasterOverlay
	>(
		this->get_material_key().utf8().get_data(),
		this->m_url.utf8().get_data(),
		this->request_headers_from_dictionary(this->m_requestHeaders),
		tmsOptions,
		options
	);
}

String CesiumTileMapServiceRasterOverlay::provider_type() const {
	return "tile_map_service";
}

void CesiumTileMapServiceRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["url"] = this->m_url;
	result["specify_zoom_levels"] = this->m_specifyZoomLevels;
	result["minimum_level"] = this->m_minimumLevel;
	result["maximum_level"] = this->m_maximumLevel;
	result["request_headers"] = this->m_requestHeaders.duplicate(true);
}

void CesiumTileMapServiceRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CesiumTileMapServiceRasterOverlay::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CesiumTileMapServiceRasterOverlay::get_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ClassDB::bind_method(D_METHOD("set_specify_zoom_levels", "value"), &CesiumTileMapServiceRasterOverlay::set_specify_zoom_levels);
	ClassDB::bind_method(D_METHOD("get_specify_zoom_levels"), &CesiumTileMapServiceRasterOverlay::get_specify_zoom_levels);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "specify_zoom_levels"), "set_specify_zoom_levels", "get_specify_zoom_levels");
	ClassDB::bind_method(D_METHOD("set_minimum_level", "value"), &CesiumTileMapServiceRasterOverlay::set_minimum_level);
	ClassDB::bind_method(D_METHOD("get_minimum_level"), &CesiumTileMapServiceRasterOverlay::get_minimum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_minimum_level", "get_minimum_level");
	ClassDB::bind_method(D_METHOD("set_maximum_level", "value"), &CesiumTileMapServiceRasterOverlay::set_maximum_level);
	ClassDB::bind_method(D_METHOD("get_maximum_level"), &CesiumTileMapServiceRasterOverlay::get_maximum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_maximum_level", "get_maximum_level");
	ClassDB::bind_method(D_METHOD("set_request_headers", "headers"), &CesiumTileMapServiceRasterOverlay::set_request_headers);
	ClassDB::bind_method(D_METHOD("get_request_headers"), &CesiumTileMapServiceRasterOverlay::get_request_headers);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "request_headers"), "set_request_headers", "get_request_headers");
}
