// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumGeoJsonObject.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GEO_JSON_OBJECT_H
#define CESIUM_GEO_JSON_OBJECT_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/math/aabb.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/packed_float64_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/aabb.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
using namespace godot;
#endif

#include <CesiumVectorData/GeoJsonDocument.h>
#include <CesiumVectorData/GeoJsonObject.h>

#include <memory>

class CesiumVectorStyle;

/** A stable view of one object in an owned GeoJSON document. */
class CesiumGeoJsonObject : public Resource {
	GDCLASS(CesiumGeoJsonObject, Resource)

public:
	enum ObjectType {
		Point = 0,
		MultiPoint = 1,
		LineString = 2,
		MultiLineString = 3,
		Polygon = 4,
		MultiPolygon = 5,
		GeometryCollection = 6,
		Feature = 7,
		FeatureCollection = 8,
		Invalid = -1,
	};

	bool is_valid() const;
	int32_t get_object_type() const;
	String get_object_type_name() const;
	bool has_bounding_box() const;
	AABB get_bounding_box() const;
	Dictionary get_foreign_members() const;

	bool has_style() const;
	Ref<CesiumVectorStyle> get_style() const;
	bool set_style(const Ref<CesiumVectorStyle>& style);
	void clear_style();

	// Geometry coordinates are flattened longitude/latitude/height triples.
	PackedFloat64Array get_points_exact() const;
	Array get_line_strings_exact() const;
	Array get_polygons_exact() const;
	Array get_children() const;

	bool is_feature() const;
	Variant get_feature_id() const;
	Dictionary get_feature_properties() const;
	Ref<CesiumGeoJsonObject> get_feature_geometry() const;

	void initialize(
		const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document,
		const CesiumVectorData::GeoJsonObject* object,
		const ObjectID& ownerDocument = ObjectID()
	);

protected:
	static void _bind_methods();

private:
	Ref<CesiumGeoJsonObject> wrap(
		const CesiumVectorData::GeoJsonObject* object
	) const;
	void notify_changed_owner();

	std::shared_ptr<CesiumVectorData::GeoJsonDocument> m_document;
	const CesiumVectorData::GeoJsonObject* m_object = nullptr;
	ObjectID m_ownerDocument;
};

VARIANT_ENUM_CAST(CesiumGeoJsonObject::ObjectType);

#endif // CESIUM_GEO_JSON_OBJECT_H
