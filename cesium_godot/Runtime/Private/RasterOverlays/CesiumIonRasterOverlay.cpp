// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumIonRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumIonRasterOverlay.h"

#include "Models/CesiumGDConfig.h"

#include <CesiumRasterOverlays/IonRasterOverlay.h>
#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>

#include <algorithm>

void CesiumIonRasterOverlay::set_asset_id(int64_t id) {
	const int64_t bounded = std::max<int64_t>(0, id);
	if (this->m_assetId == bounded) {
		return;
	}
	this->m_assetId = bounded;
	this->refresh();
}

int64_t CesiumIonRasterOverlay::get_asset_id() const {
	return this->m_assetId;
}

void CesiumIonRasterOverlay::set_ion_access_token(const String& token) {
	if (this->m_ionAccessToken == token) {
		return;
	}
	this->m_ionAccessToken = token;
	this->refresh();
}

const String& CesiumIonRasterOverlay::get_ion_access_token() const {
	return this->m_ionAccessToken;
}

void CesiumIonRasterOverlay::set_ion_api_url(const String& url) {
	if (this->m_ionApiUrl == url) {
		return;
	}
	this->m_ionApiUrl = url;
	this->refresh();
}

const String& CesiumIonRasterOverlay::get_ion_api_url() const {
	return this->m_ionApiUrl;
}

void CesiumIonRasterOverlay::set_asset_options(const String& options) {
	if (this->m_assetOptions == options) {
		return;
	}
	this->m_assetOptions = options;
	this->refresh();
}

const String& CesiumIonRasterOverlay::get_asset_options() const {
	return this->m_assetOptions;
}

void CesiumIonRasterOverlay::set_data_source(int32_t dataSource) {
	const CesiumDataSource bounded = dataSource ==
		static_cast<int32_t>(CesiumDataSource::FromUrl)
		? CesiumDataSource::FromUrl
		: CesiumDataSource::FromCesiumIon;
	if (this->m_legacyDataSource == bounded) {
		return;
	}
	this->m_legacyDataSource = bounded;
	this->notify_property_list_changed();
	this->refresh();
}

int32_t CesiumIonRasterOverlay::get_data_source() const {
	return static_cast<int32_t>(this->m_legacyDataSource);
}

void CesiumIonRasterOverlay::set_url(const String& url) {
	if (this->m_legacyTmsUrl == url) {
		return;
	}
	this->m_legacyTmsUrl = url;
	this->refresh();
}

const String& CesiumIonRasterOverlay::get_url() const {
	return this->m_legacyTmsUrl;
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumIonRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_legacyDataSource == CesiumDataSource::FromUrl) {
		if (this->m_legacyTmsUrl.is_empty()) {
			return nullptr;
		}
		return std::make_unique<
			CesiumRasterOverlays::TileMapServiceRasterOverlay
		>(
			this->get_material_key().utf8().get_data(),
			this->m_legacyTmsUrl.utf8().get_data(),
			std::vector<CesiumAsync::IAssetAccessor::THeader>{},
			CesiumRasterOverlays::TileMapServiceRasterOverlayOptions{},
			options
		);
	}
	if (this->m_assetId <= 0 || this->m_ionApiUrl.is_empty()) {
		return nullptr;
	}

	String apiUrl = this->m_ionApiUrl;
	if (!apiUrl.ends_with("/")) {
		apiUrl += "/";
	}
	const String token = this->m_ionAccessToken.is_empty()
		? CesiumGDConfig::get_singleton(this)->get_access_token()
		: this->m_ionAccessToken;
	auto overlay = std::make_unique<CesiumRasterOverlays::IonRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		this->m_assetId,
		token.utf8().get_data(),
		options,
		apiUrl.utf8().get_data()
	);
	if (!this->m_assetOptions.is_empty()) {
		overlay->setAssetOptions(this->m_assetOptions.utf8().get_data());
	}
	return overlay;
}

String CesiumIonRasterOverlay::provider_type() const {
	return this->m_legacyDataSource == CesiumDataSource::FromUrl
		? "tile_map_service_legacy"
		: "cesium_ion";
}

void CesiumIonRasterOverlay::append_provider_configuration(
	Dictionary& result
) const {
	result["asset_id"] = this->m_assetId;
	result["ion_api_url"] = this->m_ionApiUrl;
	result["asset_options"] = this->m_assetOptions;
	result["access_token_source"] = this->m_ionAccessToken.is_empty()
		? "project_default"
		: "overlay_override";
	result["legacy_data_source"] = static_cast<int32_t>(
		this->m_legacyDataSource
	);
	result["legacy_tms_url"] = this->m_legacyTmsUrl;
}

void CesiumIonRasterOverlay::_get_property_list(
	List<PropertyInfo>* properties
) const {
#if defined(CESIUM_GD_MODULE)
	for (int32_t index = 0; index < properties->size(); ++index) {
		PropertyInfo& property = properties->get(index);
#elif defined(CESIUM_GD_EXT)
	for (auto iterator = properties->begin(); iterator != properties->end(); ++iterator) {
		PropertyInfo& property = *iterator;
#endif
		if (property.name == StringName("url")) {
			property.usage = this->m_legacyDataSource == CesiumDataSource::FromUrl
				? PROPERTY_USAGE_DEFAULT
				: PROPERTY_USAGE_READ_ONLY;
		} else if (
			property.name == StringName("asset_id") ||
			property.name == StringName("ion_access_token") ||
			property.name == StringName("ion_api_url") ||
			property.name == StringName("asset_options")
		) {
			property.usage = this->m_legacyDataSource ==
				CesiumDataSource::FromCesiumIon
				? PROPERTY_USAGE_DEFAULT
				: PROPERTY_USAGE_READ_ONLY;
		}
	}
}

void CesiumIonRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_asset_id", "id"), &CesiumIonRasterOverlay::set_asset_id);
	ClassDB::bind_method(D_METHOD("get_asset_id"), &CesiumIonRasterOverlay::get_asset_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "asset_id", PROPERTY_HINT_RANGE, "0,9223372036854775807,1"), "set_asset_id", "get_asset_id");

	ClassDB::bind_method(D_METHOD("set_ion_access_token", "token"), &CesiumIonRasterOverlay::set_ion_access_token);
	ClassDB::bind_method(D_METHOD("get_ion_access_token"), &CesiumIonRasterOverlay::get_ion_access_token);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ion_access_token", PROPERTY_HINT_PASSWORD), "set_ion_access_token", "get_ion_access_token");

	ClassDB::bind_method(D_METHOD("set_ion_api_url", "url"), &CesiumIonRasterOverlay::set_ion_api_url);
	ClassDB::bind_method(D_METHOD("get_ion_api_url"), &CesiumIonRasterOverlay::get_ion_api_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ion_api_url"), "set_ion_api_url", "get_ion_api_url");

	ClassDB::bind_method(D_METHOD("set_asset_options", "options"), &CesiumIonRasterOverlay::set_asset_options);
	ClassDB::bind_method(D_METHOD("get_asset_options"), &CesiumIonRasterOverlay::get_asset_options);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "asset_options", PROPERTY_HINT_MULTILINE_TEXT), "set_asset_options", "get_asset_options");

	ClassDB::bind_method(D_METHOD("set_data_source", "data_source"), &CesiumIonRasterOverlay::set_data_source);
	ClassDB::bind_method(D_METHOD("get_data_source"), &CesiumIonRasterOverlay::get_data_source);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "data_source", PROPERTY_HINT_ENUM, "From Cesium Ion,From URL (legacy TMS)"), "set_data_source", "get_data_source");

	ClassDB::bind_method(D_METHOD("set_url", "url"), &CesiumIonRasterOverlay::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CesiumIonRasterOverlay::get_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");

	ClassDB::bind_integer_constant(get_class_static(), "CesiumDataSource", "FromCesiumIon", static_cast<int32_t>(CesiumDataSource::FromCesiumIon));
	ClassDB::bind_integer_constant(get_class_static(), "CesiumDataSource", "FromUrl", static_cast<int32_t>(CesiumDataSource::FromUrl));
}
