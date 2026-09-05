// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumGeoJsonDocumentRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GEO_JSON_DOCUMENT_RASTER_OVERLAY_H
#define CESIUM_GEO_JSON_DOCUMENT_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

#if defined(CESIUM_GD_MODULE)
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
#endif

namespace CesiumVectorData {
class GeoJsonDocument;
}

class CesiumGeoJsonDocument;
class CesiumVectorStyle;

/** Rasterizes a parsed, inline, local, remote, or ion GeoJSON document. */
class CesiumGeoJsonDocumentRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumGeoJsonDocumentRasterOverlay, CesiumRasterOverlay)

public:
	enum Source {
		FromDocument = 0,
		FromString = 1,
		FromUrl = 2,
		FromCesiumIon = 3,
	};

	CesiumGeoJsonDocumentRasterOverlay();
	~CesiumGeoJsonDocumentRasterOverlay() override;
	void _enter_tree() override;

	void set_source(int32_t source);
	int32_t get_source() const;
	void set_document(const Ref<CesiumGeoJsonDocument>& document);
	Ref<CesiumGeoJsonDocument> get_document() const;
	void set_geo_json(const String& geoJson);
	const String& get_geo_json() const;
	void set_url(const String& url);
	const String& get_url() const;
	void set_request_headers(const Dictionary& headers);
	Dictionary get_request_headers() const;
	void set_ion_asset_id(int64_t assetId);
	int64_t get_ion_asset_id() const;
	void set_ion_access_token(const String& token);
	const String& get_ion_access_token() const;
	void set_ion_api_url(const String& url);
	const String& get_ion_api_url() const;
	void set_mip_levels(int32_t levels);
	int32_t get_mip_levels() const;
	void set_default_style(const Ref<CesiumVectorStyle>& style);
	Ref<CesiumVectorStyle> get_default_style() const;
	PackedStringArray get_last_document_errors() const;
	PackedStringArray get_last_document_warnings() const;

	void _on_document_changed();
	void _on_default_style_changed();

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo>* properties) const;
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	void on_removing_from_tileset(
		Cesium3DTileset* tileset,
		CesiumRasterOverlays::RasterOverlay* overlay
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	void disconnect_document();
	void connect_document();
	void disconnect_style();
	void connect_style();
	void handle_document_result(
		uint64_t generation,
		const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document,
		const PackedStringArray& errors,
		const PackedStringArray& warnings
	);
	String resolved_url() const;

	Source m_source = FromDocument;
	Ref<CesiumGeoJsonDocument> m_document;
	String m_geoJson;
	String m_url;
	Dictionary m_requestHeaders;
	int64_t m_ionAssetId = 0;
	String m_ionAccessToken;
	String m_ionApiUrl = "https://api.cesium.com/";
	int32_t m_mipLevels = 0;
	Ref<CesiumVectorStyle> m_defaultStyle;
	PackedStringArray m_lastDocumentErrors;
	PackedStringArray m_lastDocumentWarnings;
	uint64_t m_documentGeneration = 0;
};

VARIANT_ENUM_CAST(CesiumGeoJsonDocumentRasterOverlay::Source);

#endif // CESIUM_GEO_JSON_DOCUMENT_RASTER_OVERLAY_H
