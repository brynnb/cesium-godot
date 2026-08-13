// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumBingMapsRasterOverlay.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_BING_MAPS_RASTER_OVERLAY_H
#define CESIUM_BING_MAPS_RASTER_OVERLAY_H

#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"

/** Raster imagery loaded directly from the Bing Maps imagery service. */
class CesiumBingMapsRasterOverlay : public CesiumRasterOverlay {
	GDCLASS(CesiumBingMapsRasterOverlay, CesiumRasterOverlay)

public:
	enum MapStyle {
		Aerial = 0,
		AerialWithLabelsOnDemand = 1,
		RoadOnDemand = 2,
		CanvasDark = 3,
		CanvasLight = 4,
		CanvasGray = 5,
		OrdnanceSurvey = 6,
		CollinsBart = 7,
	};

	void set_api_key(const String& value);
	const String& get_api_key() const;
	void set_map_style(int32_t value);
	int32_t get_map_style() const;
	void set_culture(const String& value);
	const String& get_culture() const;
	void set_service_url(const String& value);
	const String& get_service_url() const;

protected:
	static void _bind_methods();
	std::unique_ptr<CesiumRasterOverlays::RasterOverlay> create_overlay(
		const CesiumRasterOverlays::RasterOverlayOptions& options
	) override;
	String provider_type() const override;
	void append_provider_configuration(Dictionary& result) const override;

private:
	String m_apiKey;
	MapStyle m_mapStyle = Aerial;
	String m_culture;
	String m_serviceUrl = "https://dev.virtualearth.net";
};

VARIANT_ENUM_CAST(CesiumBingMapsRasterOverlay::MapStyle);

#endif // CESIUM_BING_MAPS_RASTER_OVERLAY_H
