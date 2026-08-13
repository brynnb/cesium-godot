// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's CesiumGeoJsonDocument.
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GEO_JSON_DOCUMENT_H
#define CESIUM_GEO_JSON_DOCUMENT_H

#if defined(CESIUM_GD_MODULE)
#include "core/io/resource.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/packed_string_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
using namespace godot;
#endif

#include <CesiumVectorData/GeoJsonDocument.h>

#include <memory>

class CesiumGeoJsonObject;

/**
 * An owned, lifetime-safe parsed GeoJSON document.
 *
 * Cesium Native remains authoritative for validation and geometry. Returned
 * object Resources retain the Native document, so they remain valid if this
 * Resource is released or an overlay generation is replaced.
 */
class CesiumGeoJsonDocument : public Resource {
	GDCLASS(CesiumGeoJsonDocument, Resource)

public:
	bool load_from_string(const String& geoJson, const Array& attributions = Array());
	void clear();
	bool is_valid() const;
	Ref<CesiumGeoJsonObject> get_root_object() const;
	Array get_objects() const;
	Dictionary get_statistics() const;
	Array get_attributions() const;
	PackedStringArray get_errors() const;
	PackedStringArray get_warnings() const;

	const std::shared_ptr<CesiumVectorData::GeoJsonDocument>&
	get_native_document() const;
	void initialize_native(
		std::shared_ptr<CesiumVectorData::GeoJsonDocument>&& document,
		const PackedStringArray& errors = PackedStringArray(),
		const PackedStringArray& warnings = PackedStringArray()
	);
	void notify_object_changed();

protected:
	static void _bind_methods();

private:
	std::shared_ptr<CesiumVectorData::GeoJsonDocument> m_document;
	PackedStringArray m_errors;
	PackedStringArray m_warnings;
};

#endif // CESIUM_GEO_JSON_DOCUMENT_H
