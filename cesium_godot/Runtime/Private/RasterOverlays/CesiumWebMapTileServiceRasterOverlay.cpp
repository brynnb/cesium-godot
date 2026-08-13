// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumWebMapTileServiceRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumWebMapTileServiceRasterOverlay.h"

#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumRasterOverlays/WebMapTileServiceRasterOverlay.h>

#include <algorithm>
#include <map>

namespace {
real_t clamp_longitude(real_t value) {
	return std::clamp<real_t>(value, -180.0, 180.0);
}

real_t clamp_latitude(real_t value) {
	return std::clamp<real_t>(value, -90.0, 90.0);
}

std::vector<std::string> packed_strings(const PackedStringArray& values) {
	std::vector<std::string> result;
	result.reserve(values.size());
	for (int64_t i = 0; i < values.size(); ++i) {
		result.emplace_back(values[i].utf8().get_data());
	}
	return result;
}

std::map<std::string, std::string> string_map(const Dictionary& values) {
	std::map<std::string, std::string> result;
	const Array keys = values.keys();
	for (int64_t i = 0; i < keys.size(); ++i) {
		const String key = String(keys[i]);
		result.emplace(
			key.utf8().get_data(),
			String(values[keys[i]]).utf8().get_data()
		);
	}
	return result;
}
} // namespace

#define CESIUM_WMTS_STRING_SETTER(method, field) \
	void CesiumWebMapTileServiceRasterOverlay::method(const String& value) { \
		if (this->field == value) return; \
		this->field = value; \
		this->refresh(); \
	}

CESIUM_WMTS_STRING_SETTER(set_base_url, m_baseUrl)
const String& CesiumWebMapTileServiceRasterOverlay::get_base_url() const { return this->m_baseUrl; }
CESIUM_WMTS_STRING_SETTER(set_layer, m_layer)
const String& CesiumWebMapTileServiceRasterOverlay::get_layer() const { return this->m_layer; }
CESIUM_WMTS_STRING_SETTER(set_style, m_style)
const String& CesiumWebMapTileServiceRasterOverlay::get_style() const { return this->m_style; }
CESIUM_WMTS_STRING_SETTER(set_format, m_format)
const String& CesiumWebMapTileServiceRasterOverlay::get_format() const { return this->m_format; }
CESIUM_WMTS_STRING_SETTER(set_tile_matrix_set_id, m_tileMatrixSetId)
const String& CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_id() const { return this->m_tileMatrixSetId; }
CESIUM_WMTS_STRING_SETTER(set_tile_matrix_set_label_prefix, m_tileMatrixSetLabelPrefix)
const String& CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_label_prefix() const { return this->m_tileMatrixSetLabelPrefix; }

void CesiumWebMapTileServiceRasterOverlay::set_specify_tile_matrix_set_labels(bool value) {
	if (this->m_specifyTileMatrixSetLabels == value) return;
	this->m_specifyTileMatrixSetLabels = value;
	this->notify_property_list_changed();
	this->refresh();
}
bool CesiumWebMapTileServiceRasterOverlay::get_specify_tile_matrix_set_labels() const { return this->m_specifyTileMatrixSetLabels; }

void CesiumWebMapTileServiceRasterOverlay::set_tile_matrix_set_labels(const PackedStringArray& value) {
	if (this->m_tileMatrixSetLabels == value) return;
	this->m_tileMatrixSetLabels = value;
	this->refresh();
}
PackedStringArray CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_labels() const { return this->m_tileMatrixSetLabels; }

void CesiumWebMapTileServiceRasterOverlay::set_projection(int32_t value) {
	const CesiumRasterOverlayProjection bounded = value == 1
		? CesiumRasterOverlayProjection::Geographic
		: CesiumRasterOverlayProjection::WebMercator;
	if (this->m_projection == bounded) return;
	this->m_projection = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_projection() const { return static_cast<int32_t>(this->m_projection); }

void CesiumWebMapTileServiceRasterOverlay::set_specify_tiling_scheme(bool value) {
	if (this->m_specifyTilingScheme == value) return;
	this->m_specifyTilingScheme = value;
	this->notify_property_list_changed();
	this->refresh();
}
bool CesiumWebMapTileServiceRasterOverlay::get_specify_tiling_scheme() const { return this->m_specifyTilingScheme; }

void CesiumWebMapTileServiceRasterOverlay::set_root_tiles_x(int32_t value) {
	const int32_t bounded = std::max(1, value);
	if (this->m_rootTilesX == bounded) return;
	this->m_rootTilesX = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_root_tiles_x() const { return this->m_rootTilesX; }

void CesiumWebMapTileServiceRasterOverlay::set_root_tiles_y(int32_t value) {
	const int32_t bounded = std::max(1, value);
	if (this->m_rootTilesY == bounded) return;
	this->m_rootTilesY = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_root_tiles_y() const { return this->m_rootTilesY; }

void CesiumWebMapTileServiceRasterOverlay::set_rectangle_west(real_t value) {
	const real_t bounded = clamp_longitude(value);
	if (Math::is_equal_approx(this->m_rectangleWest, bounded)) return;
	this->m_rectangleWest = bounded;
	this->refresh();
}
real_t CesiumWebMapTileServiceRasterOverlay::get_rectangle_west() const { return this->m_rectangleWest; }

void CesiumWebMapTileServiceRasterOverlay::set_rectangle_south(real_t value) {
	const real_t bounded = clamp_latitude(value);
	if (Math::is_equal_approx(this->m_rectangleSouth, bounded)) return;
	this->m_rectangleSouth = bounded;
	this->refresh();
}
real_t CesiumWebMapTileServiceRasterOverlay::get_rectangle_south() const { return this->m_rectangleSouth; }

void CesiumWebMapTileServiceRasterOverlay::set_rectangle_east(real_t value) {
	const real_t bounded = clamp_longitude(value);
	if (Math::is_equal_approx(this->m_rectangleEast, bounded)) return;
	this->m_rectangleEast = bounded;
	this->refresh();
}
real_t CesiumWebMapTileServiceRasterOverlay::get_rectangle_east() const { return this->m_rectangleEast; }

void CesiumWebMapTileServiceRasterOverlay::set_rectangle_north(real_t value) {
	const real_t bounded = clamp_latitude(value);
	if (Math::is_equal_approx(this->m_rectangleNorth, bounded)) return;
	this->m_rectangleNorth = bounded;
	this->refresh();
}
real_t CesiumWebMapTileServiceRasterOverlay::get_rectangle_north() const { return this->m_rectangleNorth; }

void CesiumWebMapTileServiceRasterOverlay::set_specify_zoom_levels(bool value) {
	if (this->m_specifyZoomLevels == value) return;
	this->m_specifyZoomLevels = value;
	this->notify_property_list_changed();
	this->refresh();
}
bool CesiumWebMapTileServiceRasterOverlay::get_specify_zoom_levels() const { return this->m_specifyZoomLevels; }

void CesiumWebMapTileServiceRasterOverlay::set_minimum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_minimumLevel == bounded) return;
	this->m_minimumLevel = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_minimum_level() const { return this->m_minimumLevel; }

void CesiumWebMapTileServiceRasterOverlay::set_maximum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_maximumLevel == bounded) return;
	this->m_maximumLevel = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_maximum_level() const { return this->m_maximumLevel; }

void CesiumWebMapTileServiceRasterOverlay::set_tile_width(int32_t value) {
	const int32_t bounded = std::clamp(value, 64, 2048);
	if (this->m_tileWidth == bounded) return;
	this->m_tileWidth = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_tile_width() const { return this->m_tileWidth; }

void CesiumWebMapTileServiceRasterOverlay::set_tile_height(int32_t value) {
	const int32_t bounded = std::clamp(value, 64, 2048);
	if (this->m_tileHeight == bounded) return;
	this->m_tileHeight = bounded;
	this->refresh();
}
int32_t CesiumWebMapTileServiceRasterOverlay::get_tile_height() const { return this->m_tileHeight; }

void CesiumWebMapTileServiceRasterOverlay::set_subdomains(const PackedStringArray& value) {
	if (this->m_subdomains == value) return;
	this->m_subdomains = value;
	this->refresh();
}
PackedStringArray CesiumWebMapTileServiceRasterOverlay::get_subdomains() const { return this->m_subdomains; }

void CesiumWebMapTileServiceRasterOverlay::set_dimensions(const Dictionary& value) {
	this->m_dimensions = value.duplicate(true);
	this->refresh();
}
Dictionary CesiumWebMapTileServiceRasterOverlay::get_dimensions() const { return this->m_dimensions.duplicate(true); }

void CesiumWebMapTileServiceRasterOverlay::set_request_headers(const Dictionary& value) {
	this->m_requestHeaders = value.duplicate(true);
	this->refresh();
}
Dictionary CesiumWebMapTileServiceRasterOverlay::get_request_headers() const { return this->m_requestHeaders.duplicate(true); }

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumWebMapTileServiceRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_baseUrl.is_empty()) return nullptr;

	CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions providerOptions;
	if (!this->m_format.is_empty()) providerOptions.format = this->m_format.utf8().get_data();
	providerOptions.layer = this->m_layer.utf8().get_data();
	providerOptions.style = this->m_style.utf8().get_data();
	providerOptions.tileMatrixSetID = this->m_tileMatrixSetId.utf8().get_data();
	providerOptions.tileWidth = static_cast<uint32_t>(this->m_tileWidth);
	providerOptions.tileHeight = static_cast<uint32_t>(this->m_tileHeight);
	providerOptions.subdomains = packed_strings(this->m_subdomains);
	if (!this->m_dimensions.is_empty()) providerOptions.dimensions = string_map(this->m_dimensions);

	if (this->m_specifyZoomLevels) {
		providerOptions.minimumLevel = static_cast<uint32_t>(this->m_minimumLevel);
		providerOptions.maximumLevel = static_cast<uint32_t>(std::max(this->m_minimumLevel, this->m_maximumLevel));
	}

	providerOptions.projection = this->m_projection == CesiumRasterOverlayProjection::Geographic
		? CesiumGeospatial::Projection(CesiumGeospatial::GeographicProjection(options.ellipsoid))
		: CesiumGeospatial::Projection(CesiumGeospatial::WebMercatorProjection(options.ellipsoid));

	if (this->m_specifyTilingScheme) {
		const CesiumGeospatial::GlobeRectangle globeRectangle =
			CesiumGeospatial::GlobeRectangle::fromDegrees(
				this->m_rectangleWest,
				this->m_rectangleSouth,
				this->m_rectangleEast,
				this->m_rectangleNorth
			);
		const CesiumGeometry::Rectangle coverage = CesiumGeospatial::projectRectangleSimple(
			*providerOptions.projection,
			globeRectangle
		);
		providerOptions.coverageRectangle = coverage;
		providerOptions.tilingScheme = CesiumGeometry::QuadtreeTilingScheme(
			coverage,
			this->m_rootTilesX,
			this->m_rootTilesY
		);
	}

	if (this->m_specifyTileMatrixSetLabels) {
		if (!this->m_tileMatrixSetLabels.is_empty()) {
			providerOptions.tileMatrixLabels = packed_strings(this->m_tileMatrixSetLabels);
		}
	} else if (!this->m_tileMatrixSetLabelPrefix.is_empty()) {
		const int32_t lastLevel = this->m_specifyZoomLevels
			? std::max(this->m_minimumLevel, this->m_maximumLevel)
			: 25;
		std::vector<std::string> labels;
		labels.reserve(static_cast<size_t>(lastLevel) + 1);
		for (int32_t level = 0; level <= lastLevel; ++level) {
			labels.emplace_back((this->m_tileMatrixSetLabelPrefix + String::num_int64(level)).utf8().get_data());
		}
		providerOptions.tileMatrixLabels = std::move(labels);
	}

	return std::make_unique<CesiumRasterOverlays::WebMapTileServiceRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		this->m_baseUrl.utf8().get_data(),
		this->request_headers_from_dictionary(this->m_requestHeaders),
		providerOptions,
		options
	);
}

String CesiumWebMapTileServiceRasterOverlay::provider_type() const { return "web_map_tile_service"; }

void CesiumWebMapTileServiceRasterOverlay::append_provider_configuration(Dictionary& result) const {
	result["base_url"] = this->m_baseUrl;
	result["layer"] = this->m_layer;
	result["style"] = this->m_style;
	result["format"] = this->m_format;
	result["tile_matrix_set_id"] = this->m_tileMatrixSetId;
	result["tile_matrix_set_label_prefix"] = this->m_tileMatrixSetLabelPrefix;
	result["specify_tile_matrix_set_labels"] = this->m_specifyTileMatrixSetLabels;
	result["tile_matrix_set_labels"] = this->m_tileMatrixSetLabels;
	result["projection"] = static_cast<int32_t>(this->m_projection);
	result["projection_name"] = this->m_projection == CesiumRasterOverlayProjection::Geographic ? "geographic" : "web_mercator";
	result["specify_tiling_scheme"] = this->m_specifyTilingScheme;
	result["root_tiles_x"] = this->m_rootTilesX;
	result["root_tiles_y"] = this->m_rootTilesY;
	result["rectangle_degrees"] = Vector4(this->m_rectangleWest, this->m_rectangleSouth, this->m_rectangleEast, this->m_rectangleNorth);
	result["specify_zoom_levels"] = this->m_specifyZoomLevels;
	result["minimum_level"] = this->m_minimumLevel;
	result["maximum_level"] = std::max(this->m_minimumLevel, this->m_maximumLevel);
	result["tile_width"] = this->m_tileWidth;
	result["tile_height"] = this->m_tileHeight;
	result["subdomains"] = this->m_subdomains;
	result["dimensions"] = this->m_dimensions.duplicate(true);
	result["request_headers"] = this->m_requestHeaders.duplicate(true);
}

#undef CESIUM_WMTS_STRING_SETTER

void CesiumWebMapTileServiceRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_base_url", "value"), &CesiumWebMapTileServiceRasterOverlay::set_base_url);
	ClassDB::bind_method(D_METHOD("get_base_url"), &CesiumWebMapTileServiceRasterOverlay::get_base_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_url"), "set_base_url", "get_base_url");
	ClassDB::bind_method(D_METHOD("set_layer", "value"), &CesiumWebMapTileServiceRasterOverlay::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer"), &CesiumWebMapTileServiceRasterOverlay::get_layer);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "layer"), "set_layer", "get_layer");
	ClassDB::bind_method(D_METHOD("set_style", "value"), &CesiumWebMapTileServiceRasterOverlay::set_style);
	ClassDB::bind_method(D_METHOD("get_style"), &CesiumWebMapTileServiceRasterOverlay::get_style);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "style"), "set_style", "get_style");
	ClassDB::bind_method(D_METHOD("set_format", "value"), &CesiumWebMapTileServiceRasterOverlay::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &CesiumWebMapTileServiceRasterOverlay::get_format);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "format", PROPERTY_HINT_ENUM, "image/jpeg,image/png"), "set_format", "get_format");
	ClassDB::bind_method(D_METHOD("set_tile_matrix_set_id", "value"), &CesiumWebMapTileServiceRasterOverlay::set_tile_matrix_set_id);
	ClassDB::bind_method(D_METHOD("get_tile_matrix_set_id"), &CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_id);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tile_matrix_set_id"), "set_tile_matrix_set_id", "get_tile_matrix_set_id");
	ClassDB::bind_method(D_METHOD("set_tile_matrix_set_label_prefix", "value"), &CesiumWebMapTileServiceRasterOverlay::set_tile_matrix_set_label_prefix);
	ClassDB::bind_method(D_METHOD("get_tile_matrix_set_label_prefix"), &CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_label_prefix);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tile_matrix_set_label_prefix"), "set_tile_matrix_set_label_prefix", "get_tile_matrix_set_label_prefix");
	ClassDB::bind_method(D_METHOD("set_specify_tile_matrix_set_labels", "value"), &CesiumWebMapTileServiceRasterOverlay::set_specify_tile_matrix_set_labels);
	ClassDB::bind_method(D_METHOD("get_specify_tile_matrix_set_labels"), &CesiumWebMapTileServiceRasterOverlay::get_specify_tile_matrix_set_labels);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "specify_tile_matrix_set_labels"), "set_specify_tile_matrix_set_labels", "get_specify_tile_matrix_set_labels");
	ClassDB::bind_method(D_METHOD("set_tile_matrix_set_labels", "value"), &CesiumWebMapTileServiceRasterOverlay::set_tile_matrix_set_labels);
	ClassDB::bind_method(D_METHOD("get_tile_matrix_set_labels"), &CesiumWebMapTileServiceRasterOverlay::get_tile_matrix_set_labels);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "tile_matrix_set_labels"), "set_tile_matrix_set_labels", "get_tile_matrix_set_labels");
	ClassDB::bind_method(D_METHOD("set_projection", "value"), &CesiumWebMapTileServiceRasterOverlay::set_projection);
	ClassDB::bind_method(D_METHOD("get_projection"), &CesiumWebMapTileServiceRasterOverlay::get_projection);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "projection", PROPERTY_HINT_ENUM, "Web Mercator,Geographic"), "set_projection", "get_projection");
	ClassDB::bind_integer_constant(get_class_static(), "Projection", "ProjectionWebMercator", 0);
	ClassDB::bind_integer_constant(get_class_static(), "Projection", "ProjectionGeographic", 1);
	ClassDB::bind_method(D_METHOD("set_specify_tiling_scheme", "value"), &CesiumWebMapTileServiceRasterOverlay::set_specify_tiling_scheme);
	ClassDB::bind_method(D_METHOD("get_specify_tiling_scheme"), &CesiumWebMapTileServiceRasterOverlay::get_specify_tiling_scheme);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "specify_tiling_scheme"), "set_specify_tiling_scheme", "get_specify_tiling_scheme");
	ClassDB::bind_method(D_METHOD("set_root_tiles_x", "value"), &CesiumWebMapTileServiceRasterOverlay::set_root_tiles_x);
	ClassDB::bind_method(D_METHOD("get_root_tiles_x"), &CesiumWebMapTileServiceRasterOverlay::get_root_tiles_x);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_tiles_x", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_root_tiles_x", "get_root_tiles_x");
	ClassDB::bind_method(D_METHOD("set_root_tiles_y", "value"), &CesiumWebMapTileServiceRasterOverlay::set_root_tiles_y);
	ClassDB::bind_method(D_METHOD("get_root_tiles_y"), &CesiumWebMapTileServiceRasterOverlay::get_root_tiles_y);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_tiles_y", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_root_tiles_y", "get_root_tiles_y");
	ClassDB::bind_method(D_METHOD("set_rectangle_west", "value"), &CesiumWebMapTileServiceRasterOverlay::set_rectangle_west);
	ClassDB::bind_method(D_METHOD("get_rectangle_west"), &CesiumWebMapTileServiceRasterOverlay::get_rectangle_west);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_west", PROPERTY_HINT_RANGE, "-180,180,0.000001,suffix:°"), "set_rectangle_west", "get_rectangle_west");
	ClassDB::bind_method(D_METHOD("set_rectangle_south", "value"), &CesiumWebMapTileServiceRasterOverlay::set_rectangle_south);
	ClassDB::bind_method(D_METHOD("get_rectangle_south"), &CesiumWebMapTileServiceRasterOverlay::get_rectangle_south);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_south", PROPERTY_HINT_RANGE, "-90,90,0.000001,suffix:°"), "set_rectangle_south", "get_rectangle_south");
	ClassDB::bind_method(D_METHOD("set_rectangle_east", "value"), &CesiumWebMapTileServiceRasterOverlay::set_rectangle_east);
	ClassDB::bind_method(D_METHOD("get_rectangle_east"), &CesiumWebMapTileServiceRasterOverlay::get_rectangle_east);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_east", PROPERTY_HINT_RANGE, "-180,180,0.000001,suffix:°"), "set_rectangle_east", "get_rectangle_east");
	ClassDB::bind_method(D_METHOD("set_rectangle_north", "value"), &CesiumWebMapTileServiceRasterOverlay::set_rectangle_north);
	ClassDB::bind_method(D_METHOD("get_rectangle_north"), &CesiumWebMapTileServiceRasterOverlay::get_rectangle_north);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_north", PROPERTY_HINT_RANGE, "-90,90,0.000001,suffix:°"), "set_rectangle_north", "get_rectangle_north");
	ClassDB::bind_method(D_METHOD("set_specify_zoom_levels", "value"), &CesiumWebMapTileServiceRasterOverlay::set_specify_zoom_levels);
	ClassDB::bind_method(D_METHOD("get_specify_zoom_levels"), &CesiumWebMapTileServiceRasterOverlay::get_specify_zoom_levels);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "specify_zoom_levels"), "set_specify_zoom_levels", "get_specify_zoom_levels");
	ClassDB::bind_method(D_METHOD("set_minimum_level", "value"), &CesiumWebMapTileServiceRasterOverlay::set_minimum_level);
	ClassDB::bind_method(D_METHOD("get_minimum_level"), &CesiumWebMapTileServiceRasterOverlay::get_minimum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_minimum_level", "get_minimum_level");
	ClassDB::bind_method(D_METHOD("set_maximum_level", "value"), &CesiumWebMapTileServiceRasterOverlay::set_maximum_level);
	ClassDB::bind_method(D_METHOD("get_maximum_level"), &CesiumWebMapTileServiceRasterOverlay::get_maximum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_maximum_level", "get_maximum_level");
	ClassDB::bind_method(D_METHOD("set_tile_width", "value"), &CesiumWebMapTileServiceRasterOverlay::set_tile_width);
	ClassDB::bind_method(D_METHOD("get_tile_width"), &CesiumWebMapTileServiceRasterOverlay::get_tile_width);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_width", PROPERTY_HINT_RANGE, "64,2048,1"), "set_tile_width", "get_tile_width");
	ClassDB::bind_method(D_METHOD("set_tile_height", "value"), &CesiumWebMapTileServiceRasterOverlay::set_tile_height);
	ClassDB::bind_method(D_METHOD("get_tile_height"), &CesiumWebMapTileServiceRasterOverlay::get_tile_height);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_height", PROPERTY_HINT_RANGE, "64,2048,1"), "set_tile_height", "get_tile_height");
	ClassDB::bind_method(D_METHOD("set_subdomains", "value"), &CesiumWebMapTileServiceRasterOverlay::set_subdomains);
	ClassDB::bind_method(D_METHOD("get_subdomains"), &CesiumWebMapTileServiceRasterOverlay::get_subdomains);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "subdomains"), "set_subdomains", "get_subdomains");
	ClassDB::bind_method(D_METHOD("set_dimensions", "value"), &CesiumWebMapTileServiceRasterOverlay::set_dimensions);
	ClassDB::bind_method(D_METHOD("get_dimensions"), &CesiumWebMapTileServiceRasterOverlay::get_dimensions);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "dimensions"), "set_dimensions", "get_dimensions");
	ClassDB::bind_method(D_METHOD("set_request_headers", "value"), &CesiumWebMapTileServiceRasterOverlay::set_request_headers);
	ClassDB::bind_method(D_METHOD("get_request_headers"), &CesiumWebMapTileServiceRasterOverlay::get_request_headers);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "request_headers"), "set_request_headers", "get_request_headers");
}
