// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumVectorStyle.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_VECTOR_STYLE_H
#define CESIUM_VECTOR_STYLE_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/variant/color.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/color.hpp"
using namespace godot;
#endif

#include <CesiumVectorData/VectorStyle.h>

/** Editable, reusable styling for GeoJSON lines and polygons. */
class CesiumVectorStyle : public Resource {
	GDCLASS(CesiumVectorStyle, Resource)

public:
	enum ColorMode {
		ColorNormal = 0,
		ColorRandom = 1,
	};
	enum LineWidthMode {
		WidthPixels = 0,
		WidthMeters = 1,
	};

	void set_line_color(const Color& color);
	Color get_line_color() const;
	void set_line_color_mode(int32_t mode);
	int32_t get_line_color_mode() const;
	void set_line_width(real_t width);
	real_t get_line_width() const;
	void set_line_width_mode(int32_t mode);
	int32_t get_line_width_mode() const;

	void set_polygon_fill_enabled(bool enabled);
	bool get_polygon_fill_enabled() const;
	void set_polygon_fill_color(const Color& color);
	Color get_polygon_fill_color() const;
	void set_polygon_fill_color_mode(int32_t mode);
	int32_t get_polygon_fill_color_mode() const;

	void set_polygon_outline_enabled(bool enabled);
	bool get_polygon_outline_enabled() const;
	void set_polygon_outline_color(const Color& color);
	Color get_polygon_outline_color() const;
	void set_polygon_outline_color_mode(int32_t mode);
	int32_t get_polygon_outline_color_mode() const;
	void set_polygon_outline_width(real_t width);
	real_t get_polygon_outline_width() const;
	void set_polygon_outline_width_mode(int32_t mode);
	int32_t get_polygon_outline_width_mode() const;

	CesiumVectorData::VectorStyle to_native() const;
	void initialize_from_native(const CesiumVectorData::VectorStyle& style);

protected:
	static void _bind_methods();

private:
	Color m_lineColor = Color(1.0, 1.0, 1.0, 1.0);
	ColorMode m_lineColorMode = ColorNormal;
	real_t m_lineWidth = 1.0;
	LineWidthMode m_lineWidthMode = WidthPixels;
	bool m_polygonFillEnabled = true;
	Color m_polygonFillColor = Color(1.0, 1.0, 1.0, 1.0);
	ColorMode m_polygonFillColorMode = ColorNormal;
	bool m_polygonOutlineEnabled = false;
	Color m_polygonOutlineColor = Color(1.0, 1.0, 1.0, 1.0);
	ColorMode m_polygonOutlineColorMode = ColorNormal;
	real_t m_polygonOutlineWidth = 1.0;
	LineWidthMode m_polygonOutlineWidthMode = WidthPixels;
};

VARIANT_ENUM_CAST(CesiumVectorStyle::ColorMode);
VARIANT_ENUM_CAST(CesiumVectorStyle::LineWidthMode);

#endif // CESIUM_VECTOR_STYLE_H
