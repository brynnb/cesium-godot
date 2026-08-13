// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_RASTER_OVERLAY_H
#define CESIUM_RASTER_OVERLAY_H

#if defined(CESIUM_GD_MODULE)
#include "core/variant/dictionary.h"
#include "scene/3d/node_3d.h"
#elif defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
using namespace godot;
#endif

#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <memory>

class Cesium3DTileset;
class CesiumLoadFailure;

/**
 * Base Godot node for a Cesium Native raster-overlay provider.
 *
 * Provider subclasses only translate their source-specific properties into a
 * Native RasterOverlay. This base owns activation, refresh, common budgets,
 * deferred failure delivery, and generation-safe tileset attachment.
 */
class CesiumRasterOverlay : public Node3D {
	GDCLASS(CesiumRasterOverlay, Node3D)

public:
	void set_material_key(const String& key);
	const String& get_material_key() const;

	void set_maximum_screen_space_error(real_t value);
	real_t get_maximum_screen_space_error() const;

	void set_maximum_texture_size(int32_t value);
	int32_t get_maximum_texture_size() const;

	void set_maximum_simultaneous_tile_loads(int32_t value);
	int32_t get_maximum_simultaneous_tile_loads() const;

	void set_sub_tile_cache_bytes(int64_t value);
	int64_t get_sub_tile_cache_bytes() const;

	void set_show_credits_on_screen(bool value);
	bool get_show_credits_on_screen() const;

	void set_generate_mipmaps(bool value);
	bool get_generate_mipmaps() const;

	Error add_to_tileset(Cesium3DTileset* tileset);
	void remove_from_tileset(Cesium3DTileset* tileset = nullptr);
	void refresh();

	bool is_added_to_tileset() const;
	String get_provider_type() const;
	Dictionary get_configuration() const;

	CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>
	get_overlay_instance() const;

	void emit_load_failure_deferred(const Ref<CesiumLoadFailure>& failure);
	void _ready() override;
	void _exit_tree() override;

protected:
	static void _bind_methods();

	virtual std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) = 0;
	virtual String provider_type() const = 0;
	virtual void append_provider_configuration(Dictionary& result) const = 0;
	virtual void on_added_to_tileset(
		Cesium3DTileset* tileset,
		CesiumRasterOverlays::RasterOverlay* overlay
	);
	virtual void on_removing_from_tileset(
		Cesium3DTileset* tileset,
		CesiumRasterOverlays::RasterOverlay* overlay
	);

	static std::vector<CesiumAsync::IAssetAccessor::THeader>
	request_headers_from_dictionary(const Dictionary& headers);
	const CesiumAsync::AsyncSystem* get_async_system() const;
	std::shared_ptr<CesiumAsync::IAssetAccessor> get_asset_accessor() const;

private:
	CesiumRasterOverlays::RasterOverlayOptions create_options(
		Cesium3DTileset* tileset
	) const;
	Cesium3DTileset* resolve_tileset() const;

	// Match Cesium for Unreal's default material-layer key. Generated plugin
	// materials expose this standard layer without application shader code.
	String m_materialKey = "Overlay0";
	real_t m_maximumScreenSpaceError = 2.0;
	int32_t m_maximumTextureSize = 2048;
	int32_t m_maximumSimultaneousTileLoads = 20;
	int64_t m_subTileCacheBytes = 16 * 1024 * 1024;
	bool m_showCreditsOnScreen = false;
	bool m_generateMipmaps = true;

	CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>
		m_overlayInstance;
	ObjectID m_tileset;
};

#endif // CESIUM_RASTER_OVERLAY_H
