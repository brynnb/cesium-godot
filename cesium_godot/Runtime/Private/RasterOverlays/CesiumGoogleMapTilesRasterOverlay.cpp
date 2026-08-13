// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumGoogleMapTilesRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumGoogleMapTilesRasterOverlay.h"

#include <CesiumJsonReader/JsonObjectJsonHandler.h>
#include <CesiumJsonReader/JsonReader.h>
#include <CesiumRasterOverlays/GoogleMapTilesRasterOverlay.h>
#include <CesiumUtility/JsonValue.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace {
const std::string& map_type_name(CesiumGoogleMapTilesRasterOverlay::MapType type) {
	using CesiumRasterOverlays::GoogleMapTilesMapType;
	switch (type) {
		case CesiumGoogleMapTilesRasterOverlay::Roadmap:
			return GoogleMapTilesMapType::roadmap;
		case CesiumGoogleMapTilesRasterOverlay::Terrain:
			return GoogleMapTilesMapType::terrain;
		case CesiumGoogleMapTilesRasterOverlay::Satellite:
		default:
			return GoogleMapTilesMapType::satellite;
	}
}

const std::string& scale_name(CesiumGoogleMapTilesRasterOverlay::Scale scale) {
	using CesiumRasterOverlays::GoogleMapTilesScale;
	switch (scale) {
		case CesiumGoogleMapTilesRasterOverlay::Scale2x:
			return GoogleMapTilesScale::scaleFactor2x;
		case CesiumGoogleMapTilesRasterOverlay::Scale4x:
			return GoogleMapTilesScale::scaleFactor4x;
		case CesiumGoogleMapTilesRasterOverlay::Scale1x:
		default:
			return GoogleMapTilesScale::scaleFactor1x;
	}
}

String style_diagnostic(int32_t index, const std::string& message) {
	return String("Style ") + String::num_int64(index) + ": " + message.c_str();
}
} // namespace

#define CESIUM_GOOGLE_STRING_SETTER(method, field) \
	void CesiumGoogleMapTilesRasterOverlay::method(const String& value) { \
		if (this->field == value) return; \
		this->field = value; \
		this->refresh(); \
	}

CESIUM_GOOGLE_STRING_SETTER(set_api_key, m_apiKey)
const String& CesiumGoogleMapTilesRasterOverlay::get_api_key() const {
	return this->m_apiKey;
}

void CesiumGoogleMapTilesRasterOverlay::set_map_type(int32_t value) {
	const MapType bounded = static_cast<MapType>(std::clamp(value, 0, 2));
	if (this->m_mapType == bounded) return;
	this->m_mapType = bounded;
	this->refresh();
}

int32_t CesiumGoogleMapTilesRasterOverlay::get_map_type() const {
	return static_cast<int32_t>(this->m_mapType);
}

CESIUM_GOOGLE_STRING_SETTER(set_language, m_language)
const String& CesiumGoogleMapTilesRasterOverlay::get_language() const {
	return this->m_language;
}
CESIUM_GOOGLE_STRING_SETTER(set_region, m_region)
const String& CesiumGoogleMapTilesRasterOverlay::get_region() const {
	return this->m_region;
}

void CesiumGoogleMapTilesRasterOverlay::set_image_format(int32_t value) {
	const ImageFormat bounded = static_cast<ImageFormat>(std::clamp(value, 0, 2));
	if (this->m_imageFormat == bounded) return;
	this->m_imageFormat = bounded;
	this->refresh();
}

int32_t CesiumGoogleMapTilesRasterOverlay::get_image_format() const {
	return static_cast<int32_t>(this->m_imageFormat);
}

void CesiumGoogleMapTilesRasterOverlay::set_scale(int32_t value) {
	const Scale bounded = static_cast<Scale>(std::clamp(value, 0, 2));
	if (this->m_scale == bounded) return;
	this->m_scale = bounded;
	this->refresh();
}

int32_t CesiumGoogleMapTilesRasterOverlay::get_scale() const {
	return static_cast<int32_t>(this->m_scale);
}

void CesiumGoogleMapTilesRasterOverlay::set_high_dpi(bool value) {
	if (this->m_highDpi == value) return;
	this->m_highDpi = value;
	this->refresh();
}

bool CesiumGoogleMapTilesRasterOverlay::get_high_dpi() const {
	return this->m_highDpi;
}

void CesiumGoogleMapTilesRasterOverlay::set_layer_types(int32_t value) {
	const int32_t bounded = value & (LayerRoadmap | LayerStreetView | LayerTraffic);
	if (this->m_layerTypes == bounded) return;
	this->m_layerTypes = bounded;
	this->refresh();
}

int32_t CesiumGoogleMapTilesRasterOverlay::get_layer_types() const {
	return this->m_layerTypes;
}

void CesiumGoogleMapTilesRasterOverlay::set_styles(const PackedStringArray& value) {
	if (this->m_styles == value) return;
	this->m_styles = value;
	this->m_lastStyleErrors.clear();
	this->refresh();
}

PackedStringArray CesiumGoogleMapTilesRasterOverlay::get_styles() const {
	return this->m_styles;
}

void CesiumGoogleMapTilesRasterOverlay::set_overlay(bool value) {
	if (this->m_overlay == value) return;
	this->m_overlay = value;
	this->refresh();
}

bool CesiumGoogleMapTilesRasterOverlay::get_overlay() const {
	return this->m_overlay;
}

CESIUM_GOOGLE_STRING_SETTER(set_api_base_url, m_apiBaseUrl)
const String& CesiumGoogleMapTilesRasterOverlay::get_api_base_url() const {
	return this->m_apiBaseUrl;
}

PackedStringArray CesiumGoogleMapTilesRasterOverlay::get_last_style_errors() const {
	return this->m_lastStyleErrors;
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumGoogleMapTilesRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_apiKey.is_empty() || this->m_apiBaseUrl.is_empty()) return nullptr;

	CesiumRasterOverlays::GoogleMapTilesNewSessionParameters parameters;
	parameters.key = this->m_apiKey.utf8().get_data();
	parameters.mapType = map_type_name(this->m_mapType);
	parameters.language = this->m_language.utf8().get_data();
	parameters.region = this->m_region.utf8().get_data();
	if (this->m_imageFormat == Png) {
		parameters.imageFormat = CesiumRasterOverlays::GoogleMapTilesImageFormat::png;
	} else if (this->m_imageFormat == Jpeg) {
		parameters.imageFormat = CesiumRasterOverlays::GoogleMapTilesImageFormat::jpeg;
	}
	parameters.scale = scale_name(this->m_scale);
	parameters.highDpi = this->m_highDpi;

	std::vector<std::string> layers;
	if ((this->m_layerTypes & LayerRoadmap) != 0) {
		layers.emplace_back(CesiumRasterOverlays::GoogleMapTilesLayerType::layerRoadmap);
	}
	if ((this->m_layerTypes & LayerStreetView) != 0) {
		layers.emplace_back(CesiumRasterOverlays::GoogleMapTilesLayerType::layerStreetview);
	}
	if ((this->m_layerTypes & LayerTraffic) != 0) {
		layers.emplace_back(CesiumRasterOverlays::GoogleMapTilesLayerType::layerTraffic);
	}
	parameters.layerTypes = std::move(layers);

	this->m_lastStyleErrors.clear();
	CesiumUtility::JsonValue::Array styles;
	styles.reserve(this->m_styles.size());
	for (int32_t index = 0; index < this->m_styles.size(); ++index) {
		const CharString utf8 = this->m_styles[index].utf8();
		CesiumJsonReader::JsonObjectJsonHandler handler;
		auto result = CesiumJsonReader::JsonReader::readJson(
			std::span<const std::byte>(
				reinterpret_cast<const std::byte*>(utf8.get_data()),
				static_cast<size_t>(utf8.length())
			),
			handler
		);
		for (const std::string& error : result.errors) {
			this->m_lastStyleErrors.push_back(style_diagnostic(index, error));
		}
		for (const std::string& warning : result.warnings) {
			this->m_lastStyleErrors.push_back(style_diagnostic(index, warning));
		}
		if (!result.value) continue;
		if (!result.value->isObject()) {
			this->m_lastStyleErrors.push_back(
				style_diagnostic(index, "must be a JSON object")
			);
			continue;
		}
		styles.emplace_back(std::move(*result.value));
	}
	parameters.styles = std::move(styles);
	parameters.overlay = this->m_overlay;
	parameters.apiBaseUrl = this->m_apiBaseUrl.utf8().get_data();

	return std::make_unique<CesiumRasterOverlays::GoogleMapTilesRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		parameters,
		options
	);
}

String CesiumGoogleMapTilesRasterOverlay::provider_type() const {
	return "google_map_tiles";
}

void CesiumGoogleMapTilesRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["api_base_url"] = this->m_apiBaseUrl;
	result["map_type"] = static_cast<int32_t>(this->m_mapType);
	result["map_type_name"] = map_type_name(this->m_mapType).c_str();
	result["language"] = this->m_language;
	result["region"] = this->m_region;
	result["image_format"] = static_cast<int32_t>(this->m_imageFormat);
	result["scale"] = static_cast<int32_t>(this->m_scale);
	result["scale_name"] = scale_name(this->m_scale).c_str();
	result["high_dpi"] = this->m_highDpi;
	result["layer_types"] = this->m_layerTypes;
	result["style_count"] = this->m_styles.size();
	result["style_error_count"] = this->m_lastStyleErrors.size();
	result["overlay"] = this->m_overlay;
	result["api_key_configured"] = !this->m_apiKey.is_empty();
}

#undef CESIUM_GOOGLE_STRING_SETTER

void CesiumGoogleMapTilesRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_api_key", "value"), &CesiumGoogleMapTilesRasterOverlay::set_api_key);
	ClassDB::bind_method(D_METHOD("get_api_key"), &CesiumGoogleMapTilesRasterOverlay::get_api_key);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "api_key", PROPERTY_HINT_PASSWORD), "set_api_key", "get_api_key");

	ClassDB::bind_method(D_METHOD("set_map_type", "value"), &CesiumGoogleMapTilesRasterOverlay::set_map_type);
	ClassDB::bind_method(D_METHOD("get_map_type"), &CesiumGoogleMapTilesRasterOverlay::get_map_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "map_type", PROPERTY_HINT_ENUM, "Satellite,Roadmap,Terrain"), "set_map_type", "get_map_type");

	ClassDB::bind_method(D_METHOD("set_language", "value"), &CesiumGoogleMapTilesRasterOverlay::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &CesiumGoogleMapTilesRasterOverlay::get_language);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language"), "set_language", "get_language");

	ClassDB::bind_method(D_METHOD("set_region", "value"), &CesiumGoogleMapTilesRasterOverlay::set_region);
	ClassDB::bind_method(D_METHOD("get_region"), &CesiumGoogleMapTilesRasterOverlay::get_region);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "region"), "set_region", "get_region");

	ClassDB::bind_method(D_METHOD("set_image_format", "value"), &CesiumGoogleMapTilesRasterOverlay::set_image_format);
	ClassDB::bind_method(D_METHOD("get_image_format"), &CesiumGoogleMapTilesRasterOverlay::get_image_format);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "image_format", PROPERTY_HINT_ENUM, "Automatic,PNG,JPEG"), "set_image_format", "get_image_format");

	ClassDB::bind_method(D_METHOD("set_scale", "value"), &CesiumGoogleMapTilesRasterOverlay::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &CesiumGoogleMapTilesRasterOverlay::get_scale);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scale", PROPERTY_HINT_ENUM, "1x,2x,4x"), "set_scale", "get_scale");

	ClassDB::bind_method(D_METHOD("set_high_dpi", "value"), &CesiumGoogleMapTilesRasterOverlay::set_high_dpi);
	ClassDB::bind_method(D_METHOD("get_high_dpi"), &CesiumGoogleMapTilesRasterOverlay::get_high_dpi);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "high_dpi"), "set_high_dpi", "get_high_dpi");

	ClassDB::bind_method(D_METHOD("set_layer_types", "value"), &CesiumGoogleMapTilesRasterOverlay::set_layer_types);
	ClassDB::bind_method(D_METHOD("get_layer_types"), &CesiumGoogleMapTilesRasterOverlay::get_layer_types);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer_types", PROPERTY_HINT_FLAGS, "Roadmap,Street View,Traffic"), "set_layer_types", "get_layer_types");

	ClassDB::bind_method(D_METHOD("set_styles", "value"), &CesiumGoogleMapTilesRasterOverlay::set_styles);
	ClassDB::bind_method(D_METHOD("get_styles"), &CesiumGoogleMapTilesRasterOverlay::get_styles);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "styles"), "set_styles", "get_styles");

	ClassDB::bind_method(D_METHOD("set_overlay", "value"), &CesiumGoogleMapTilesRasterOverlay::set_overlay);
	ClassDB::bind_method(D_METHOD("get_overlay"), &CesiumGoogleMapTilesRasterOverlay::get_overlay);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "overlay"), "set_overlay", "get_overlay");

	ClassDB::bind_method(D_METHOD("set_api_base_url", "value"), &CesiumGoogleMapTilesRasterOverlay::set_api_base_url);
	ClassDB::bind_method(D_METHOD("get_api_base_url"), &CesiumGoogleMapTilesRasterOverlay::get_api_base_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "api_base_url"), "set_api_base_url", "get_api_base_url");

	ClassDB::bind_method(D_METHOD("get_last_style_errors"), &CesiumGoogleMapTilesRasterOverlay::get_last_style_errors);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "last_style_errors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "", "get_last_style_errors");

	BIND_ENUM_CONSTANT(Satellite);
	BIND_ENUM_CONSTANT(Roadmap);
	BIND_ENUM_CONSTANT(Terrain);
	BIND_ENUM_CONSTANT(Automatic);
	BIND_ENUM_CONSTANT(Png);
	BIND_ENUM_CONSTANT(Jpeg);
	BIND_ENUM_CONSTANT(Scale1x);
	BIND_ENUM_CONSTANT(Scale2x);
	BIND_ENUM_CONSTANT(Scale4x);
	BIND_ENUM_CONSTANT(LayerRoadmap);
	BIND_ENUM_CONSTANT(LayerStreetView);
	BIND_ENUM_CONSTANT(LayerTraffic);
}
