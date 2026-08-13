// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumBingMapsRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumBingMapsRasterOverlay.h"

#include <CesiumRasterOverlays/BingMapsRasterOverlay.h>

#include <algorithm>

namespace {
const std::string& map_style_name(CesiumBingMapsRasterOverlay::MapStyle style) {
	using CesiumRasterOverlays::BingMapsStyle;
	switch (style) {
		case CesiumBingMapsRasterOverlay::AerialWithLabelsOnDemand:
			return BingMapsStyle::AERIAL_WITH_LABELS_ON_DEMAND;
		case CesiumBingMapsRasterOverlay::RoadOnDemand:
			return BingMapsStyle::ROAD_ON_DEMAND;
		case CesiumBingMapsRasterOverlay::CanvasDark:
			return BingMapsStyle::CANVAS_DARK;
		case CesiumBingMapsRasterOverlay::CanvasLight:
			return BingMapsStyle::CANVAS_LIGHT;
		case CesiumBingMapsRasterOverlay::CanvasGray:
			return BingMapsStyle::CANVAS_GRAY;
		case CesiumBingMapsRasterOverlay::OrdnanceSurvey:
			return BingMapsStyle::ORDNANCE_SURVEY;
		case CesiumBingMapsRasterOverlay::CollinsBart:
			return BingMapsStyle::COLLINS_BART;
		case CesiumBingMapsRasterOverlay::Aerial:
		default:
			return BingMapsStyle::AERIAL;
	}
}
} // namespace

void CesiumBingMapsRasterOverlay::set_api_key(const String& value) {
	if (this->m_apiKey == value) return;
	this->m_apiKey = value;
	this->refresh();
}

const String& CesiumBingMapsRasterOverlay::get_api_key() const {
	return this->m_apiKey;
}

void CesiumBingMapsRasterOverlay::set_map_style(int32_t value) {
	const MapStyle bounded = static_cast<MapStyle>(std::clamp(value, 0, 7));
	if (this->m_mapStyle == bounded) return;
	this->m_mapStyle = bounded;
	this->refresh();
}

int32_t CesiumBingMapsRasterOverlay::get_map_style() const {
	return static_cast<int32_t>(this->m_mapStyle);
}

void CesiumBingMapsRasterOverlay::set_culture(const String& value) {
	if (this->m_culture == value) return;
	this->m_culture = value;
	this->refresh();
}

const String& CesiumBingMapsRasterOverlay::get_culture() const {
	return this->m_culture;
}

void CesiumBingMapsRasterOverlay::set_service_url(const String& value) {
	if (this->m_serviceUrl == value) return;
	this->m_serviceUrl = value;
	this->refresh();
}

const String& CesiumBingMapsRasterOverlay::get_service_url() const {
	return this->m_serviceUrl;
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumBingMapsRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_apiKey.is_empty() || this->m_serviceUrl.is_empty()) return nullptr;

	return std::make_unique<CesiumRasterOverlays::BingMapsRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		this->m_serviceUrl.utf8().get_data(),
		this->m_apiKey.utf8().get_data(),
		map_style_name(this->m_mapStyle),
		this->m_culture.utf8().get_data(),
		options
	);
}

String CesiumBingMapsRasterOverlay::provider_type() const {
	return "bing_maps";
}

void CesiumBingMapsRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["service_url"] = this->m_serviceUrl;
	result["map_style"] = static_cast<int32_t>(this->m_mapStyle);
	result["map_style_name"] = map_style_name(this->m_mapStyle).c_str();
	result["culture"] = this->m_culture;
	result["api_key_configured"] = !this->m_apiKey.is_empty();
}

void CesiumBingMapsRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_api_key", "value"), &CesiumBingMapsRasterOverlay::set_api_key);
	ClassDB::bind_method(D_METHOD("get_api_key"), &CesiumBingMapsRasterOverlay::get_api_key);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "api_key", PROPERTY_HINT_PASSWORD), "set_api_key", "get_api_key");

	ClassDB::bind_method(D_METHOD("set_map_style", "value"), &CesiumBingMapsRasterOverlay::set_map_style);
	ClassDB::bind_method(D_METHOD("get_map_style"), &CesiumBingMapsRasterOverlay::get_map_style);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_style", PROPERTY_HINT_ENUM, "Aerial,Aerial With Labels,Road,Canvas Dark,Canvas Light,Canvas Gray,Ordnance Survey,Collins Bart"), "set_map_style", "get_map_style");

	ClassDB::bind_method(D_METHOD("set_culture", "value"), &CesiumBingMapsRasterOverlay::set_culture);
	ClassDB::bind_method(D_METHOD("get_culture"), &CesiumBingMapsRasterOverlay::get_culture);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "culture"), "set_culture", "get_culture");

	ClassDB::bind_method(D_METHOD("set_service_url", "value"), &CesiumBingMapsRasterOverlay::set_service_url);
	ClassDB::bind_method(D_METHOD("get_service_url"), &CesiumBingMapsRasterOverlay::get_service_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "service_url"), "set_service_url", "get_service_url");

	BIND_ENUM_CONSTANT(Aerial);
	BIND_ENUM_CONSTANT(AerialWithLabelsOnDemand);
	BIND_ENUM_CONSTANT(RoadOnDemand);
	BIND_ENUM_CONSTANT(CanvasDark);
	BIND_ENUM_CONSTANT(CanvasLight);
	BIND_ENUM_CONSTANT(CanvasGray);
	BIND_ENUM_CONSTANT(OrdnanceSurvey);
	BIND_ENUM_CONSTANT(CollinsBart);
}
