// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumVectorStyle.cpp.

#include "Runtime/Public/Vector/CesiumVectorStyle.h"

#include <CesiumUtility/Color.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {
uint8_t color_component(real_t value) {
	return static_cast<uint8_t>(std::clamp<long>(
		std::lround(std::clamp<double>(value, 0.0, 1.0) * 255.0),
		0,
		255
	));
}

CesiumUtility::Color native_color(const Color& color) {
	return CesiumUtility::Color(
		color_component(color.r),
		color_component(color.g),
		color_component(color.b),
		color_component(color.a)
	);
}

CesiumVectorData::ColorMode native_color_mode(int32_t mode) {
	return mode == CesiumVectorStyle::ColorRandom
		? CesiumVectorData::ColorMode::Random
		: CesiumVectorData::ColorMode::Normal;
}

CesiumVectorData::LineWidthMode native_width_mode(int32_t mode) {
	return mode == CesiumVectorStyle::WidthMeters
		? CesiumVectorData::LineWidthMode::Meters
		: CesiumVectorData::LineWidthMode::Pixels;
}
} // namespace

#define CESIUM_STYLE_SETTER(method, field, type) \
	void CesiumVectorStyle::method(type value) { \
		if (this->field == value) return; \
		this->field = value; \
		this->emit_changed(); \
	}

CESIUM_STYLE_SETTER(set_line_color, m_lineColor, const Color&)
Color CesiumVectorStyle::get_line_color() const { return this->m_lineColor; }

void CesiumVectorStyle::set_line_color_mode(int32_t mode) {
	const ColorMode bounded = mode == ColorRandom ? ColorRandom : ColorNormal;
	if (this->m_lineColorMode == bounded) return;
	this->m_lineColorMode = bounded;
	this->emit_changed();
}
int32_t CesiumVectorStyle::get_line_color_mode() const { return this->m_lineColorMode; }

void CesiumVectorStyle::set_line_width(real_t width) {
	const real_t bounded = std::isfinite(static_cast<double>(width))
		? std::max<real_t>(0.0, width)
		: 0.0;
	if (Math::is_equal_approx(this->m_lineWidth, bounded)) return;
	this->m_lineWidth = bounded;
	this->emit_changed();
}
real_t CesiumVectorStyle::get_line_width() const { return this->m_lineWidth; }

void CesiumVectorStyle::set_line_width_mode(int32_t mode) {
	const LineWidthMode bounded = mode == WidthMeters ? WidthMeters : WidthPixels;
	if (this->m_lineWidthMode == bounded) return;
	this->m_lineWidthMode = bounded;
	this->emit_changed();
}
int32_t CesiumVectorStyle::get_line_width_mode() const { return this->m_lineWidthMode; }

CESIUM_STYLE_SETTER(set_polygon_fill_enabled, m_polygonFillEnabled, bool)
bool CesiumVectorStyle::get_polygon_fill_enabled() const { return this->m_polygonFillEnabled; }
CESIUM_STYLE_SETTER(set_polygon_fill_color, m_polygonFillColor, const Color&)
Color CesiumVectorStyle::get_polygon_fill_color() const { return this->m_polygonFillColor; }

void CesiumVectorStyle::set_polygon_fill_color_mode(int32_t mode) {
	const ColorMode bounded = mode == ColorRandom ? ColorRandom : ColorNormal;
	if (this->m_polygonFillColorMode == bounded) return;
	this->m_polygonFillColorMode = bounded;
	this->emit_changed();
}
int32_t CesiumVectorStyle::get_polygon_fill_color_mode() const { return this->m_polygonFillColorMode; }

CESIUM_STYLE_SETTER(set_polygon_outline_enabled, m_polygonOutlineEnabled, bool)
bool CesiumVectorStyle::get_polygon_outline_enabled() const { return this->m_polygonOutlineEnabled; }
CESIUM_STYLE_SETTER(set_polygon_outline_color, m_polygonOutlineColor, const Color&)
Color CesiumVectorStyle::get_polygon_outline_color() const { return this->m_polygonOutlineColor; }

void CesiumVectorStyle::set_polygon_outline_color_mode(int32_t mode) {
	const ColorMode bounded = mode == ColorRandom ? ColorRandom : ColorNormal;
	if (this->m_polygonOutlineColorMode == bounded) return;
	this->m_polygonOutlineColorMode = bounded;
	this->emit_changed();
}
int32_t CesiumVectorStyle::get_polygon_outline_color_mode() const { return this->m_polygonOutlineColorMode; }

void CesiumVectorStyle::set_polygon_outline_width(real_t width) {
	const real_t bounded = std::isfinite(static_cast<double>(width))
		? std::max<real_t>(0.0, width)
		: 0.0;
	if (Math::is_equal_approx(this->m_polygonOutlineWidth, bounded)) return;
	this->m_polygonOutlineWidth = bounded;
	this->emit_changed();
}
real_t CesiumVectorStyle::get_polygon_outline_width() const { return this->m_polygonOutlineWidth; }

void CesiumVectorStyle::set_polygon_outline_width_mode(int32_t mode) {
	const LineWidthMode bounded = mode == WidthMeters ? WidthMeters : WidthPixels;
	if (this->m_polygonOutlineWidthMode == bounded) return;
	this->m_polygonOutlineWidthMode = bounded;
	this->emit_changed();
}
int32_t CesiumVectorStyle::get_polygon_outline_width_mode() const { return this->m_polygonOutlineWidthMode; }

CesiumVectorData::VectorStyle CesiumVectorStyle::to_native() const {
	CesiumVectorData::LineStyle line;
	line.color = native_color(this->m_lineColor);
	line.colorMode = native_color_mode(this->m_lineColorMode);
	line.width = static_cast<double>(this->m_lineWidth);
	line.widthMode = native_width_mode(this->m_lineWidthMode);

	CesiumVectorData::PolygonStyle polygon;
	if (this->m_polygonFillEnabled) {
		polygon.fill = CesiumVectorData::ColorStyle{
			native_color(this->m_polygonFillColor),
			native_color_mode(this->m_polygonFillColorMode)
		};
	}
	if (this->m_polygonOutlineEnabled) {
		polygon.outline = CesiumVectorData::LineStyle{
			{
				native_color(this->m_polygonOutlineColor),
				native_color_mode(this->m_polygonOutlineColorMode)
			},
			static_cast<double>(this->m_polygonOutlineWidth),
			native_width_mode(this->m_polygonOutlineWidthMode)
		};
	}
	return CesiumVectorData::VectorStyle(line, polygon);
}

void CesiumVectorStyle::initialize_from_native(
	const CesiumVectorData::VectorStyle& style
) {
	auto to_godot_color = [](const CesiumUtility::Color& color) {
		return Color(
			static_cast<real_t>(color.r) / 255.0,
			static_cast<real_t>(color.g) / 255.0,
			static_cast<real_t>(color.b) / 255.0,
			static_cast<real_t>(color.a) / 255.0
		);
	};
	this->m_lineColor = to_godot_color(style.line.color);
	this->m_lineColorMode = style.line.colorMode == CesiumVectorData::ColorMode::Random
		? ColorRandom : ColorNormal;
	this->m_lineWidth = static_cast<real_t>(style.line.width);
	this->m_lineWidthMode = style.line.widthMode == CesiumVectorData::LineWidthMode::Meters
		? WidthMeters : WidthPixels;
	this->m_polygonFillEnabled = style.polygon.fill.has_value();
	if (style.polygon.fill) {
		this->m_polygonFillColor = to_godot_color(style.polygon.fill->color);
		this->m_polygonFillColorMode = style.polygon.fill->colorMode ==
			CesiumVectorData::ColorMode::Random ? ColorRandom : ColorNormal;
	}
	this->m_polygonOutlineEnabled = style.polygon.outline.has_value();
	if (style.polygon.outline) {
		this->m_polygonOutlineColor = to_godot_color(style.polygon.outline->color);
		this->m_polygonOutlineColorMode = style.polygon.outline->colorMode ==
			CesiumVectorData::ColorMode::Random ? ColorRandom : ColorNormal;
		this->m_polygonOutlineWidth = static_cast<real_t>(style.polygon.outline->width);
		this->m_polygonOutlineWidthMode = style.polygon.outline->widthMode ==
			CesiumVectorData::LineWidthMode::Meters ? WidthMeters : WidthPixels;
	}
}

#undef CESIUM_STYLE_SETTER

void CesiumVectorStyle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_line_color", "color"), &CesiumVectorStyle::set_line_color);
	ClassDB::bind_method(D_METHOD("get_line_color"), &CesiumVectorStyle::get_line_color);
	ClassDB::bind_method(D_METHOD("set_line_color_mode", "mode"), &CesiumVectorStyle::set_line_color_mode);
	ClassDB::bind_method(D_METHOD("get_line_color_mode"), &CesiumVectorStyle::get_line_color_mode);
	ClassDB::bind_method(D_METHOD("set_line_width", "width"), &CesiumVectorStyle::set_line_width);
	ClassDB::bind_method(D_METHOD("get_line_width"), &CesiumVectorStyle::get_line_width);
	ClassDB::bind_method(D_METHOD("set_line_width_mode", "mode"), &CesiumVectorStyle::set_line_width_mode);
	ClassDB::bind_method(D_METHOD("get_line_width_mode"), &CesiumVectorStyle::get_line_width_mode);
	ADD_GROUP("Line", "line_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "line_color"), "set_line_color", "get_line_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "line_color_mode", PROPERTY_HINT_ENUM, "Normal,Random"), "set_line_color_mode", "get_line_color_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "line_width", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater"), "set_line_width", "get_line_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "line_width_mode", PROPERTY_HINT_ENUM, "Pixels,Meters"), "set_line_width_mode", "get_line_width_mode");

	ClassDB::bind_method(D_METHOD("set_polygon_fill_enabled", "enabled"), &CesiumVectorStyle::set_polygon_fill_enabled);
	ClassDB::bind_method(D_METHOD("get_polygon_fill_enabled"), &CesiumVectorStyle::get_polygon_fill_enabled);
	ClassDB::bind_method(D_METHOD("set_polygon_fill_color", "color"), &CesiumVectorStyle::set_polygon_fill_color);
	ClassDB::bind_method(D_METHOD("get_polygon_fill_color"), &CesiumVectorStyle::get_polygon_fill_color);
	ClassDB::bind_method(D_METHOD("set_polygon_fill_color_mode", "mode"), &CesiumVectorStyle::set_polygon_fill_color_mode);
	ClassDB::bind_method(D_METHOD("get_polygon_fill_color_mode"), &CesiumVectorStyle::get_polygon_fill_color_mode);
	ADD_GROUP("Polygon Fill", "polygon_fill_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "polygon_fill_enabled"), "set_polygon_fill_enabled", "get_polygon_fill_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "polygon_fill_color"), "set_polygon_fill_color", "get_polygon_fill_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "polygon_fill_color_mode", PROPERTY_HINT_ENUM, "Normal,Random"), "set_polygon_fill_color_mode", "get_polygon_fill_color_mode");

	ClassDB::bind_method(D_METHOD("set_polygon_outline_enabled", "enabled"), &CesiumVectorStyle::set_polygon_outline_enabled);
	ClassDB::bind_method(D_METHOD("get_polygon_outline_enabled"), &CesiumVectorStyle::get_polygon_outline_enabled);
	ClassDB::bind_method(D_METHOD("set_polygon_outline_color", "color"), &CesiumVectorStyle::set_polygon_outline_color);
	ClassDB::bind_method(D_METHOD("get_polygon_outline_color"), &CesiumVectorStyle::get_polygon_outline_color);
	ClassDB::bind_method(D_METHOD("set_polygon_outline_color_mode", "mode"), &CesiumVectorStyle::set_polygon_outline_color_mode);
	ClassDB::bind_method(D_METHOD("get_polygon_outline_color_mode"), &CesiumVectorStyle::get_polygon_outline_color_mode);
	ClassDB::bind_method(D_METHOD("set_polygon_outline_width", "width"), &CesiumVectorStyle::set_polygon_outline_width);
	ClassDB::bind_method(D_METHOD("get_polygon_outline_width"), &CesiumVectorStyle::get_polygon_outline_width);
	ClassDB::bind_method(D_METHOD("set_polygon_outline_width_mode", "mode"), &CesiumVectorStyle::set_polygon_outline_width_mode);
	ClassDB::bind_method(D_METHOD("get_polygon_outline_width_mode"), &CesiumVectorStyle::get_polygon_outline_width_mode);
	ADD_GROUP("Polygon Outline", "polygon_outline_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "polygon_outline_enabled"), "set_polygon_outline_enabled", "get_polygon_outline_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "polygon_outline_color"), "set_polygon_outline_color", "get_polygon_outline_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "polygon_outline_color_mode", PROPERTY_HINT_ENUM, "Normal,Random"), "set_polygon_outline_color_mode", "get_polygon_outline_color_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "polygon_outline_width", PROPERTY_HINT_RANGE, "0,1024,0.1,or_greater"), "set_polygon_outline_width", "get_polygon_outline_width");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "polygon_outline_width_mode", PROPERTY_HINT_ENUM, "Pixels,Meters"), "set_polygon_outline_width_mode", "get_polygon_outline_width_mode");

	BIND_ENUM_CONSTANT(ColorNormal);
	BIND_ENUM_CONSTANT(ColorRandom);
	BIND_ENUM_CONSTANT(WidthPixels);
	BIND_ENUM_CONSTANT(WidthMeters);
}
