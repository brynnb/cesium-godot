// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumUrlTemplateRasterOverlay.cpp.

#include "Runtime/Public/RasterOverlays/CesiumUrlTemplateRasterOverlay.h"

#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumRasterOverlays/UrlTemplateRasterOverlay.h>

#include <algorithm>

namespace {
real_t clamp_longitude(real_t value) {
	return std::clamp<real_t>(value, -180.0, 180.0);
}

real_t clamp_latitude(real_t value) {
	return std::clamp<real_t>(value, -90.0, 90.0);
}
} // namespace

#define CESIUM_REFRESH_SETTER(method, field, type) \
	void CesiumUrlTemplateRasterOverlay::method(type value) { \
		if (this->field == value) return; \
		this->field = value; \
		this->refresh(); \
	}

void CesiumUrlTemplateRasterOverlay::set_template_url(const String& url) {
	if (this->m_templateUrl == url) return;
	this->m_templateUrl = url;
	this->refresh();
}

const String& CesiumUrlTemplateRasterOverlay::get_template_url() const {
	return this->m_templateUrl;
}

void CesiumUrlTemplateRasterOverlay::set_projection(int32_t projection) {
	const CesiumRasterOverlayProjection bounded = projection == 1
		? CesiumRasterOverlayProjection::Geographic
		: CesiumRasterOverlayProjection::WebMercator;
	if (this->m_projection == bounded) return;
	this->m_projection = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_projection() const {
	return static_cast<int32_t>(this->m_projection);
}

CESIUM_REFRESH_SETTER(set_specify_tiling_scheme, m_specifyTilingScheme, bool)

bool CesiumUrlTemplateRasterOverlay::get_specify_tiling_scheme() const {
	return this->m_specifyTilingScheme;
}

void CesiumUrlTemplateRasterOverlay::set_root_tiles_x(int32_t value) {
	const int32_t bounded = std::max(1, value);
	if (this->m_rootTilesX == bounded) return;
	this->m_rootTilesX = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_root_tiles_x() const {
	return this->m_rootTilesX;
}

void CesiumUrlTemplateRasterOverlay::set_root_tiles_y(int32_t value) {
	const int32_t bounded = std::max(1, value);
	if (this->m_rootTilesY == bounded) return;
	this->m_rootTilesY = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_root_tiles_y() const {
	return this->m_rootTilesY;
}

void CesiumUrlTemplateRasterOverlay::set_rectangle_west(real_t value) {
	const real_t bounded = clamp_longitude(value);
	if (Math::is_equal_approx(this->m_rectangleWest, bounded)) return;
	this->m_rectangleWest = bounded;
	this->refresh();
}

real_t CesiumUrlTemplateRasterOverlay::get_rectangle_west() const { return this->m_rectangleWest; }

void CesiumUrlTemplateRasterOverlay::set_rectangle_south(real_t value) {
	const real_t bounded = clamp_latitude(value);
	if (Math::is_equal_approx(this->m_rectangleSouth, bounded)) return;
	this->m_rectangleSouth = bounded;
	this->refresh();
}

real_t CesiumUrlTemplateRasterOverlay::get_rectangle_south() const { return this->m_rectangleSouth; }

void CesiumUrlTemplateRasterOverlay::set_rectangle_east(real_t value) {
	const real_t bounded = clamp_longitude(value);
	if (Math::is_equal_approx(this->m_rectangleEast, bounded)) return;
	this->m_rectangleEast = bounded;
	this->refresh();
}

real_t CesiumUrlTemplateRasterOverlay::get_rectangle_east() const { return this->m_rectangleEast; }

void CesiumUrlTemplateRasterOverlay::set_rectangle_north(real_t value) {
	const real_t bounded = clamp_latitude(value);
	if (Math::is_equal_approx(this->m_rectangleNorth, bounded)) return;
	this->m_rectangleNorth = bounded;
	this->refresh();
}

real_t CesiumUrlTemplateRasterOverlay::get_rectangle_north() const { return this->m_rectangleNorth; }

void CesiumUrlTemplateRasterOverlay::set_minimum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_minimumLevel == bounded) return;
	this->m_minimumLevel = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_minimum_level() const { return this->m_minimumLevel; }

void CesiumUrlTemplateRasterOverlay::set_maximum_level(int32_t value) {
	const int32_t bounded = std::max(0, value);
	if (this->m_maximumLevel == bounded) return;
	this->m_maximumLevel = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_maximum_level() const { return this->m_maximumLevel; }

void CesiumUrlTemplateRasterOverlay::set_tile_width(int32_t value) {
	const int32_t bounded = std::clamp(value, 1, 16384);
	if (this->m_tileWidth == bounded) return;
	this->m_tileWidth = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_tile_width() const { return this->m_tileWidth; }

void CesiumUrlTemplateRasterOverlay::set_tile_height(int32_t value) {
	const int32_t bounded = std::clamp(value, 1, 16384);
	if (this->m_tileHeight == bounded) return;
	this->m_tileHeight = bounded;
	this->refresh();
}

int32_t CesiumUrlTemplateRasterOverlay::get_tile_height() const { return this->m_tileHeight; }

void CesiumUrlTemplateRasterOverlay::set_request_headers(const Dictionary& headers) {
	this->m_requestHeaders = headers.duplicate(true);
	this->refresh();
}

Dictionary CesiumUrlTemplateRasterOverlay::get_request_headers() const {
	return this->m_requestHeaders.duplicate(true);
}

std::unique_ptr<CesiumRasterOverlays::RasterOverlay>
CesiumUrlTemplateRasterOverlay::create_overlay(
	const CesiumRasterOverlays::RasterOverlayOptions& options
) {
	if (this->m_templateUrl.is_empty()) return nullptr;
	CesiumRasterOverlays::UrlTemplateRasterOverlayOptions providerOptions;
	providerOptions.minimumLevel = static_cast<uint32_t>(this->m_minimumLevel);
	providerOptions.maximumLevel = static_cast<uint32_t>(
		std::max(this->m_minimumLevel, this->m_maximumLevel)
	);
	providerOptions.tileWidth = static_cast<uint32_t>(this->m_tileWidth);
	providerOptions.tileHeight = static_cast<uint32_t>(this->m_tileHeight);
	providerOptions.projection = this->m_projection ==
		CesiumRasterOverlayProjection::Geographic
		? CesiumGeospatial::Projection(
			CesiumGeospatial::GeographicProjection(options.ellipsoid)
		)
		: CesiumGeospatial::Projection(
			CesiumGeospatial::WebMercatorProjection(options.ellipsoid)
		);

	if (this->m_specifyTilingScheme) {
		const CesiumGeospatial::GlobeRectangle globeRectangle =
			CesiumGeospatial::GlobeRectangle::fromDegrees(
				this->m_rectangleWest,
				this->m_rectangleSouth,
				this->m_rectangleEast,
				this->m_rectangleNorth
			);
		const CesiumGeometry::Rectangle coverage =
			CesiumGeospatial::projectRectangleSimple(
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

	return std::make_unique<CesiumRasterOverlays::UrlTemplateRasterOverlay>(
		this->get_material_key().utf8().get_data(),
		this->m_templateUrl.utf8().get_data(),
		this->request_headers_from_dictionary(this->m_requestHeaders),
		providerOptions,
		options
	);
}

String CesiumUrlTemplateRasterOverlay::provider_type() const { return "url_template"; }

void CesiumUrlTemplateRasterOverlay::append_provider_configuration(Dictionary& result) const {
	result["template_url"] = this->m_templateUrl;
	result["projection"] = static_cast<int32_t>(this->m_projection);
	result["projection_name"] = this->m_projection == CesiumRasterOverlayProjection::Geographic ? "geographic" : "web_mercator";
	result["specify_tiling_scheme"] = this->m_specifyTilingScheme;
	result["root_tiles_x"] = this->m_rootTilesX;
	result["root_tiles_y"] = this->m_rootTilesY;
	result["rectangle_degrees"] = Vector4(this->m_rectangleWest, this->m_rectangleSouth, this->m_rectangleEast, this->m_rectangleNorth);
	result["minimum_level"] = this->m_minimumLevel;
	result["maximum_level"] = std::max(this->m_minimumLevel, this->m_maximumLevel);
	result["tile_width"] = this->m_tileWidth;
	result["tile_height"] = this->m_tileHeight;
	result["request_headers"] = this->m_requestHeaders.duplicate(true);
}

#undef CESIUM_REFRESH_SETTER

void CesiumUrlTemplateRasterOverlay::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_template_url", "url"), &CesiumUrlTemplateRasterOverlay::set_template_url);
	ClassDB::bind_method(D_METHOD("get_template_url"), &CesiumUrlTemplateRasterOverlay::get_template_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "template_url"), "set_template_url", "get_template_url");
	ClassDB::bind_method(D_METHOD("set_projection", "projection"), &CesiumUrlTemplateRasterOverlay::set_projection);
	ClassDB::bind_method(D_METHOD("get_projection"), &CesiumUrlTemplateRasterOverlay::get_projection);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "projection", PROPERTY_HINT_ENUM, "Web Mercator,Geographic"), "set_projection", "get_projection");
	ClassDB::bind_integer_constant(get_class_static(), "Projection", "ProjectionWebMercator", 0);
	ClassDB::bind_integer_constant(get_class_static(), "Projection", "ProjectionGeographic", 1);
	ClassDB::bind_method(D_METHOD("set_specify_tiling_scheme", "value"), &CesiumUrlTemplateRasterOverlay::set_specify_tiling_scheme);
	ClassDB::bind_method(D_METHOD("get_specify_tiling_scheme"), &CesiumUrlTemplateRasterOverlay::get_specify_tiling_scheme);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "specify_tiling_scheme"), "set_specify_tiling_scheme", "get_specify_tiling_scheme");
	ClassDB::bind_method(D_METHOD("set_root_tiles_x", "value"), &CesiumUrlTemplateRasterOverlay::set_root_tiles_x);
	ClassDB::bind_method(D_METHOD("get_root_tiles_x"), &CesiumUrlTemplateRasterOverlay::get_root_tiles_x);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_tiles_x", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_root_tiles_x", "get_root_tiles_x");
	ClassDB::bind_method(D_METHOD("set_root_tiles_y", "value"), &CesiumUrlTemplateRasterOverlay::set_root_tiles_y);
	ClassDB::bind_method(D_METHOD("get_root_tiles_y"), &CesiumUrlTemplateRasterOverlay::get_root_tiles_y);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_tiles_y", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_root_tiles_y", "get_root_tiles_y");
	ClassDB::bind_method(D_METHOD("set_rectangle_west", "value"), &CesiumUrlTemplateRasterOverlay::set_rectangle_west);
	ClassDB::bind_method(D_METHOD("get_rectangle_west"), &CesiumUrlTemplateRasterOverlay::get_rectangle_west);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_west", PROPERTY_HINT_RANGE, "-180,180,0.000001,suffix:°"), "set_rectangle_west", "get_rectangle_west");
	ClassDB::bind_method(D_METHOD("set_rectangle_south", "value"), &CesiumUrlTemplateRasterOverlay::set_rectangle_south);
	ClassDB::bind_method(D_METHOD("get_rectangle_south"), &CesiumUrlTemplateRasterOverlay::get_rectangle_south);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_south", PROPERTY_HINT_RANGE, "-90,90,0.000001,suffix:°"), "set_rectangle_south", "get_rectangle_south");
	ClassDB::bind_method(D_METHOD("set_rectangle_east", "value"), &CesiumUrlTemplateRasterOverlay::set_rectangle_east);
	ClassDB::bind_method(D_METHOD("get_rectangle_east"), &CesiumUrlTemplateRasterOverlay::get_rectangle_east);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_east", PROPERTY_HINT_RANGE, "-180,180,0.000001,suffix:°"), "set_rectangle_east", "get_rectangle_east");
	ClassDB::bind_method(D_METHOD("set_rectangle_north", "value"), &CesiumUrlTemplateRasterOverlay::set_rectangle_north);
	ClassDB::bind_method(D_METHOD("get_rectangle_north"), &CesiumUrlTemplateRasterOverlay::get_rectangle_north);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rectangle_north", PROPERTY_HINT_RANGE, "-90,90,0.000001,suffix:°"), "set_rectangle_north", "get_rectangle_north");
	ClassDB::bind_method(D_METHOD("set_minimum_level", "value"), &CesiumUrlTemplateRasterOverlay::set_minimum_level);
	ClassDB::bind_method(D_METHOD("get_minimum_level"), &CesiumUrlTemplateRasterOverlay::get_minimum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minimum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_minimum_level", "get_minimum_level");
	ClassDB::bind_method(D_METHOD("set_maximum_level", "value"), &CesiumUrlTemplateRasterOverlay::set_maximum_level);
	ClassDB::bind_method(D_METHOD("get_maximum_level"), &CesiumUrlTemplateRasterOverlay::get_maximum_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_level", PROPERTY_HINT_RANGE, "0,30,1,or_greater"), "set_maximum_level", "get_maximum_level");
	ClassDB::bind_method(D_METHOD("set_tile_width", "value"), &CesiumUrlTemplateRasterOverlay::set_tile_width);
	ClassDB::bind_method(D_METHOD("get_tile_width"), &CesiumUrlTemplateRasterOverlay::get_tile_width);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_width", PROPERTY_HINT_RANGE, "1,16384,1"), "set_tile_width", "get_tile_width");
	ClassDB::bind_method(D_METHOD("set_tile_height", "value"), &CesiumUrlTemplateRasterOverlay::set_tile_height);
	ClassDB::bind_method(D_METHOD("get_tile_height"), &CesiumUrlTemplateRasterOverlay::get_tile_height);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_height", PROPERTY_HINT_RANGE, "1,16384,1"), "set_tile_height", "get_tile_height");
	ClassDB::bind_method(D_METHOD("set_request_headers", "headers"), &CesiumUrlTemplateRasterOverlay::set_request_headers);
	ClassDB::bind_method(D_METHOD("get_request_headers"), &CesiumUrlTemplateRasterOverlay::get_request_headers);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "request_headers"), "set_request_headers", "get_request_headers");
}
