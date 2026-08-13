// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumIonRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_ION_RASTER_OVERLAY_H
#define CESIUM_ION_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"
#include "Models/CesiumDataSource.h"

/** Raster imagery loaded from Cesium ion. */
class CesiumIonRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumIonRasterOverlay, CesiumRasterOverlay)

public:
	void set_asset_id(int64_t id);
	int64_t get_asset_id() const;

	void set_ion_access_token(const String& token);
	const String& get_ion_access_token() const;

	void set_ion_api_url(const String& url);
	const String& get_ion_api_url() const;

	void set_asset_options(const String& options);
	const String& get_asset_options() const;

	// Compatibility with the original Battle Road node, which combined ion
	// imagery and a URL-backed TMS provider. New projects should use
	// CesiumTileMapServiceRasterOverlay for the latter.
	void set_data_source(int32_t dataSource);
	int32_t get_data_source() const;
	void set_url(const String& url);
	const String& get_url() const;

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo>* properties) const;

	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	int64_t m_assetId = 0;
	String m_ionAccessToken;
	String m_ionApiUrl = "https://api.cesium.com/";
	String m_assetOptions;
	CesiumDataSource m_legacyDataSource = CesiumDataSource::FromCesiumIon;
	String m_legacyTmsUrl;
};

#endif // CESIUM_ION_RASTER_OVERLAY_H
