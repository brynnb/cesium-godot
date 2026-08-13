// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"
#include "Runtime/Private/RasterOverlays/CesiumRasterOverlayRendererOptions.h"
#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"

#include <CesiumAsync/IAssetResponse.h>

#include <algorithm>

namespace {
bool is_retryable_overlay_status(int32_t status) {
	return status == 0 || status == 408 || status == 425 || status == 429 ||
		status == 500 || status == 502 || status == 503 || status == 504 ||
		(status >= 520 && status <= 527);
}
} // namespace

void CesiumRasterOverlay::set_material_key(const String& key) {
	if (this->m_materialKey == key) {
		return;
	}
	this->m_materialKey = key;
	this->refresh();
}

const String& CesiumRasterOverlay::get_material_key() const {
	return this->m_materialKey;
}

void CesiumRasterOverlay::set_maximum_screen_space_error(real_t value) {
	const real_t bounded = std::max<real_t>(0.0, value);
	if (Math::is_equal_approx(this->m_maximumScreenSpaceError, bounded)) {
		return;
	}
	this->m_maximumScreenSpaceError = bounded;
	this->refresh();
}

real_t CesiumRasterOverlay::get_maximum_screen_space_error() const {
	return this->m_maximumScreenSpaceError;
}

void CesiumRasterOverlay::set_maximum_texture_size(int32_t value) {
	const int32_t bounded = std::clamp(value, 1, 16384);
	if (this->m_maximumTextureSize == bounded) {
		return;
	}
	this->m_maximumTextureSize = bounded;
	this->refresh();
}

int32_t CesiumRasterOverlay::get_maximum_texture_size() const {
	return this->m_maximumTextureSize;
}

void CesiumRasterOverlay::set_maximum_simultaneous_tile_loads(int32_t value) {
	const int32_t bounded = std::clamp(value, 1, 1024);
	this->m_maximumSimultaneousTileLoads = bounded;
	if (this->m_overlayInstance != nullptr) {
		this->m_overlayInstance->getOptions().maximumSimultaneousTileLoads =
			bounded;
	}
}

int32_t CesiumRasterOverlay::get_maximum_simultaneous_tile_loads() const {
	return this->m_maximumSimultaneousTileLoads;
}

void CesiumRasterOverlay::set_sub_tile_cache_bytes(int64_t value) {
	const int64_t bounded = std::max<int64_t>(0, value);
	this->m_subTileCacheBytes = bounded;
	if (this->m_overlayInstance != nullptr) {
		this->m_overlayInstance->getOptions().subTileCacheBytes = bounded;
	}
}

int64_t CesiumRasterOverlay::get_sub_tile_cache_bytes() const {
	return this->m_subTileCacheBytes;
}

void CesiumRasterOverlay::set_show_credits_on_screen(bool value) {
	if (this->m_showCreditsOnScreen == value) {
		return;
	}
	this->m_showCreditsOnScreen = value;
	this->refresh();
}

bool CesiumRasterOverlay::get_show_credits_on_screen() const {
	return this->m_showCreditsOnScreen;
}

void CesiumRasterOverlay::set_generate_mipmaps(bool value) {
	if (this->m_generateMipmaps == value) {
		return;
	}
	this->m_generateMipmaps = value;
	this->refresh();
}

bool CesiumRasterOverlay::get_generate_mipmaps() const {
	return this->m_generateMipmaps;
}

Error CesiumRasterOverlay::add_to_tileset(Cesium3DTileset* tileset) {
	if (tileset == nullptr) {
		return Error::ERR_INVALID_PARAMETER;
	}
	if (!tileset->has_active_tileset()) {
		return Error::ERR_UNAVAILABLE;
	}
	if (this->m_materialKey.is_empty()) {
		return Error::ERR_INVALID_PARAMETER;
	}
	if (this->m_overlayInstance != nullptr) {
		return Error::OK;
	}

	// Make the explicit owner available while a provider constructs its Native
	// overlay. This matters when add_to_tileset is called before the provider is
	// parented under the tileset (for example in tests or custom scene wiring).
	this->m_tileset = tileset->get_instance_id();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> overlay =
		this->create_overlay(this->create_options(tileset));
	if (overlay == nullptr) {
		this->m_tileset = ObjectID();
		return Error::ERR_CANT_CREATE;
	}
	this->m_overlayInstance = overlay.release();
	tileset->add_overlay(this);
	this->on_added_to_tileset(tileset, this->m_overlayInstance.get());
	return Error::OK;
}

void CesiumRasterOverlay::remove_from_tileset(Cesium3DTileset* tileset) {
	if (this->m_overlayInstance == nullptr) {
		this->m_tileset = ObjectID();
		return;
	}
	if (tileset == nullptr) {
		tileset = this->resolve_tileset();
	}
	if (tileset != nullptr) {
		this->on_removing_from_tileset(
			tileset,
			this->m_overlayInstance.get()
		);
		tileset->remove_overlay(this);
	}
	this->m_overlayInstance.reset();
	this->m_tileset = ObjectID();
}

void CesiumRasterOverlay::refresh() {
	Cesium3DTileset* tileset = this->resolve_tileset();
	if (tileset == nullptr) {
		return;
	}
	this->remove_from_tileset(tileset);
	this->add_to_tileset(tileset);
}

bool CesiumRasterOverlay::is_added_to_tileset() const {
	return this->m_overlayInstance != nullptr && this->resolve_tileset() != nullptr;
}

String CesiumRasterOverlay::get_provider_type() const {
	return this->provider_type();
}

Dictionary CesiumRasterOverlay::get_configuration() const {
	Dictionary result;
	result["provider_type"] = this->provider_type();
	result["material_key"] = this->m_materialKey;
	result["maximum_screen_space_error"] = this->m_maximumScreenSpaceError;
	result["maximum_texture_size"] = this->m_maximumTextureSize;
	result["maximum_simultaneous_tile_loads"] =
		this->m_maximumSimultaneousTileLoads;
	result["sub_tile_cache_bytes"] = this->m_subTileCacheBytes;
	result["show_credits_on_screen"] = this->m_showCreditsOnScreen;
	result["generate_mipmaps"] = this->m_generateMipmaps;
	result["added_to_tileset"] = this->is_added_to_tileset();
	this->append_provider_configuration(result);
	return result;
}

CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>
CesiumRasterOverlay::get_overlay_instance() const {
	return this->m_overlayInstance;
}

void CesiumRasterOverlay::on_added_to_tileset(
	Cesium3DTileset* tileset,
	CesiumRasterOverlays::RasterOverlay* overlay
) {
	(void)tileset;
	(void)overlay;
}

void CesiumRasterOverlay::on_removing_from_tileset(
	Cesium3DTileset* tileset,
	CesiumRasterOverlays::RasterOverlay* overlay
) {
	(void)tileset;
	(void)overlay;
}

void CesiumRasterOverlay::emit_load_failure_deferred(
	const Ref<CesiumLoadFailure>& failure
) {
	this->emit_signal("load_failure", failure);
}

void CesiumRasterOverlay::_ready() {
	Cesium3DTileset* tileset = Object::cast_to<Cesium3DTileset>(this->get_parent());
	if (tileset != nullptr && tileset->has_active_tileset()) {
		this->add_to_tileset(tileset);
	}
}

void CesiumRasterOverlay::_exit_tree() {
	this->remove_from_tileset();
}

std::vector<CesiumAsync::IAssetAccessor::THeader>
CesiumRasterOverlay::request_headers_from_dictionary(const Dictionary& headers) {
	std::vector<CesiumAsync::IAssetAccessor::THeader> result;
	const Array keys = headers.keys();
	result.reserve(static_cast<size_t>(keys.size()));
	for (int32_t index = 0; index < keys.size(); ++index) {
		const String key = static_cast<String>(keys[index]);
		const String value = static_cast<String>(headers[keys[index]]);
		if (key.is_empty()) {
			continue;
		}
		result.emplace_back(key.utf8().get_data(), value.utf8().get_data());
	}
	return result;
}

const CesiumAsync::AsyncSystem* CesiumRasterOverlay::get_async_system() const {
	Cesium3DTileset* tileset = this->resolve_tileset();
	return tileset == nullptr ? nullptr : tileset->get_native_async_system();
}

std::shared_ptr<CesiumAsync::IAssetAccessor>
CesiumRasterOverlay::get_asset_accessor() const {
	Cesium3DTileset* tileset = this->resolve_tileset();
	return tileset == nullptr
		? std::shared_ptr<CesiumAsync::IAssetAccessor>()
		: tileset->get_native_asset_accessor();
}

CesiumRasterOverlays::RasterOverlayOptions CesiumRasterOverlay::create_options(
	Cesium3DTileset* tileset
) const {
	CesiumRasterOverlays::RasterOverlayOptions options;
	options.ellipsoid = tileset->get_raster_overlay_ellipsoid();
	options.maximumScreenSpaceError = this->m_maximumScreenSpaceError;
	options.maximumTextureSize = this->m_maximumTextureSize;
	options.maximumSimultaneousTileLoads = this->m_maximumSimultaneousTileLoads;
	options.subTileCacheBytes = this->m_subTileCacheBytes;
	options.showCreditsOnScreen = this->m_showCreditsOnScreen;
	options.rendererOptions = CesiumRasterOverlayRendererOptions{
		this->m_generateMipmaps
	};

	const std::shared_ptr<CesiumLoadFailureQueue> failureQueue =
		tileset->get_load_failure_queue();
	const uint64_t sourceInstanceId = static_cast<uint64_t>(
		this->get_instance_id()
	);
	const std::string overlayKey = this->m_materialKey.utf8().get_data();
	options.loadErrorCallback = [
		failureQueue,
		sourceInstanceId,
		overlayKey
	](const CesiumRasterOverlays::RasterOverlayLoadFailureDetails& details) {
		if (failureQueue == nullptr) {
			return;
		}
		CesiumLoadFailureRecord record;
		record.sourceInstanceId = sourceInstanceId;
		record.category = CesiumLoadFailure::Category::RasterOverlay;
		record.stage =
			details.type == CesiumRasterOverlays::RasterOverlayLoadType::CesiumIon
			? CesiumLoadFailure::Stage::IonEndpoint
			: details.type ==
				CesiumRasterOverlays::RasterOverlayLoadType::TileProvider
				? CesiumLoadFailure::Stage::RasterTileProvider
				: CesiumLoadFailure::Stage::StageUnknown;
		record.message = details.message;
		record.overlayKey = overlayKey;
		if (details.pRequest != nullptr) {
			record.url = redact_cesium_diagnostic_url(details.pRequest->url());
			if (details.pRequest->response() != nullptr) {
				record.httpStatusCode = static_cast<int32_t>(
					details.pRequest->response()->statusCode()
				);
			}
		}
		record.retryable = is_retryable_overlay_status(record.httpStatusCode);
		failureQueue->push(std::move(record));
	};
	return options;
}

Cesium3DTileset* CesiumRasterOverlay::resolve_tileset() const {
	if (!this->m_tileset.is_null()) {
		return Object::cast_to<Cesium3DTileset>(
			ObjectDB::get_instance(this->m_tileset)
		);
	}
	return Object::cast_to<Cesium3DTileset>(this->get_parent());
}

void CesiumRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material_key", "key"), &CesiumRasterOverlay::set_material_key);
	ClassDB::bind_method(D_METHOD("get_material_key"), &CesiumRasterOverlay::get_material_key);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "key"), "set_material_key", "get_material_key");

	ClassDB::bind_method(D_METHOD("set_maximum_screen_space_error", "value"), &CesiumRasterOverlay::set_maximum_screen_space_error);
	ClassDB::bind_method(D_METHOD("get_maximum_screen_space_error"), &CesiumRasterOverlay::get_maximum_screen_space_error);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "maximum_screen_space_error", PROPERTY_HINT_RANGE, "0,64,0.1,or_greater"), "set_maximum_screen_space_error", "get_maximum_screen_space_error");

	ClassDB::bind_method(D_METHOD("set_maximum_texture_size", "value"), &CesiumRasterOverlay::set_maximum_texture_size);
	ClassDB::bind_method(D_METHOD("get_maximum_texture_size"), &CesiumRasterOverlay::get_maximum_texture_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_texture_size", PROPERTY_HINT_RANGE, "1,16384,1"), "set_maximum_texture_size", "get_maximum_texture_size");

	ClassDB::bind_method(D_METHOD("set_maximum_simultaneous_tile_loads", "value"), &CesiumRasterOverlay::set_maximum_simultaneous_tile_loads);
	ClassDB::bind_method(D_METHOD("get_maximum_simultaneous_tile_loads"), &CesiumRasterOverlay::get_maximum_simultaneous_tile_loads);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_simultaneous_tile_loads", PROPERTY_HINT_RANGE, "1,1024,1"), "set_maximum_simultaneous_tile_loads", "get_maximum_simultaneous_tile_loads");

	ClassDB::bind_method(D_METHOD("set_sub_tile_cache_bytes", "value"), &CesiumRasterOverlay::set_sub_tile_cache_bytes);
	ClassDB::bind_method(D_METHOD("get_sub_tile_cache_bytes"), &CesiumRasterOverlay::get_sub_tile_cache_bytes);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sub_tile_cache_bytes", PROPERTY_HINT_RANGE, "0,1073741824,1048576,or_greater,suffix:B"), "set_sub_tile_cache_bytes", "get_sub_tile_cache_bytes");

	ClassDB::bind_method(D_METHOD("set_show_credits_on_screen", "value"), &CesiumRasterOverlay::set_show_credits_on_screen);
	ClassDB::bind_method(D_METHOD("get_show_credits_on_screen"), &CesiumRasterOverlay::get_show_credits_on_screen);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_credits_on_screen"), "set_show_credits_on_screen", "get_show_credits_on_screen");

	ClassDB::bind_method(D_METHOD("set_generate_mipmaps", "value"), &CesiumRasterOverlay::set_generate_mipmaps);
	ClassDB::bind_method(D_METHOD("get_generate_mipmaps"), &CesiumRasterOverlay::get_generate_mipmaps);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_mipmaps"), "set_generate_mipmaps", "get_generate_mipmaps");

	ClassDB::bind_method(D_METHOD("add_to_tileset", "tileset"), &CesiumRasterOverlay::add_to_tileset);
	ClassDB::bind_method(D_METHOD("remove_from_tileset", "tileset"), &CesiumRasterOverlay::remove_from_tileset, DEFVAL(nullptr));
	ClassDB::bind_method(D_METHOD("refresh"), &CesiumRasterOverlay::refresh);
	ClassDB::bind_method(D_METHOD("is_added_to_tileset"), &CesiumRasterOverlay::is_added_to_tileset);
	ClassDB::bind_method(D_METHOD("get_provider_type"), &CesiumRasterOverlay::get_provider_type);
	ClassDB::bind_method(D_METHOD("get_configuration"), &CesiumRasterOverlay::get_configuration);
	ClassDB::bind_method(D_METHOD("_emit_load_failure_deferred", "failure"), &CesiumRasterOverlay::emit_load_failure_deferred);
	ADD_SIGNAL(MethodInfo("load_failure", PropertyInfo(Variant::OBJECT, "failure", PROPERTY_HINT_RESOURCE_TYPE, "CesiumLoadFailure")));
}
