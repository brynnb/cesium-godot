// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumGeoJsonDocument.cpp.

#include "Runtime/Public/Vector/CesiumGeoJsonDocument.h"

#include "Runtime/Public/Vector/CesiumGeoJsonObject.h"

#include <CesiumUtility/Result.h>

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
PackedStringArray to_strings(const std::vector<std::string>& source) {
	PackedStringArray result;
	result.resize(static_cast<int64_t>(source.size()));
	for (size_t index = 0; index < source.size(); ++index) {
		result.set(static_cast<int64_t>(index), String::utf8(source[index].c_str()));
	}
	return result;
}

Ref<CesiumGeoJsonObject> wrap_object(
	const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document,
	const CesiumVectorData::GeoJsonObject* object,
	const ObjectID& owner
) {
	Ref<CesiumGeoJsonObject> result;
	if (document == nullptr || object == nullptr) return result;
	result.instantiate();
	result->initialize(document, object, owner);
	return result;
}
} // namespace

bool CesiumGeoJsonDocument::load_from_string(
	const String& geoJson,
	const Array& attributions
) {
	std::vector<CesiumVectorData::VectorDocumentAttribution> nativeAttributions;
	nativeAttributions.reserve(static_cast<size_t>(attributions.size()));
	for (int32_t index = 0; index < attributions.size(); ++index) {
		const Variant entry = attributions[index];
		if (entry.get_type() == Variant::STRING) {
			nativeAttributions.push_back({String(entry).utf8().get_data(), false});
		} else if (entry.get_type() == Variant::DICTIONARY) {
			const Dictionary attribution = entry;
			const String html = attribution.get("html", "");
			if (!html.is_empty()) {
				nativeAttributions.push_back({
					html.utf8().get_data(),
					static_cast<bool>(attribution.get("show_on_screen", false))
				});
			}
		}
	}

	const CharString utf8 = geoJson.utf8();
	const std::span<const std::byte> bytes(
		reinterpret_cast<const std::byte*>(utf8.get_data()),
		static_cast<size_t>(utf8.length())
	);
	auto result = CesiumVectorData::GeoJsonDocument::fromGeoJson(
		bytes,
		std::move(nativeAttributions)
	);
	this->m_errors = to_strings(result.errors.errors);
	this->m_warnings = to_strings(result.errors.warnings);
	this->m_document = result.value
		? std::make_shared<CesiumVectorData::GeoJsonDocument>(
			std::move(*result.value)
		)
		: std::shared_ptr<CesiumVectorData::GeoJsonDocument>();
	this->emit_changed();
	return this->m_document != nullptr;
}

void CesiumGeoJsonDocument::clear() {
	if (this->m_document == nullptr && this->m_errors.is_empty() &&
		this->m_warnings.is_empty()) return;
	this->m_document.reset();
	this->m_errors.clear();
	this->m_warnings.clear();
	this->emit_changed();
}

bool CesiumGeoJsonDocument::is_valid() const {
	return this->m_document != nullptr;
}

Ref<CesiumGeoJsonObject> CesiumGeoJsonDocument::get_root_object() const {
	return this->m_document == nullptr
		? Ref<CesiumGeoJsonObject>()
		: wrap_object(
			this->m_document,
			&this->m_document->rootObject,
			ObjectID(this->get_instance_id())
		);
}

Array CesiumGeoJsonDocument::get_objects() const {
	Array result;
	if (this->m_document == nullptr) return result;
	for (const CesiumVectorData::GeoJsonObject& object :
		 this->m_document->rootObject) {
		result.push_back(wrap_object(
			this->m_document,
			&object,
			ObjectID(this->get_instance_id())
		));
	}
	return result;
}

Dictionary CesiumGeoJsonDocument::get_statistics() const {
	Dictionary result;
	int64_t counts[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
	int64_t styled = 0;
	if (this->m_document != nullptr) {
		for (const CesiumVectorData::GeoJsonObject& object :
			 this->m_document->rootObject) {
			const int32_t type = static_cast<int32_t>(object.getType());
			if (type >= 0 && type < 9) ++counts[type];
			if (object.getStyle().has_value()) ++styled;
		}
	}
	result["valid"] = this->is_valid();
	result["objects"] = counts[0] + counts[1] + counts[2] + counts[3] +
		counts[4] + counts[5] + counts[6] + counts[7] + counts[8];
	result["points"] = counts[0];
	result["multi_points"] = counts[1];
	result["line_strings"] = counts[2];
	result["multi_line_strings"] = counts[3];
	result["polygons"] = counts[4];
	result["multi_polygons"] = counts[5];
	result["geometry_collections"] = counts[6];
	result["features"] = counts[7];
	result["feature_collections"] = counts[8];
	result["objects_with_style"] = styled;
	result["errors"] = this->m_errors.size();
	result["warnings"] = this->m_warnings.size();
	return result;
}

Array CesiumGeoJsonDocument::get_attributions() const {
	Array result;
	if (this->m_document == nullptr) return result;
	for (const CesiumVectorData::VectorDocumentAttribution& attribution :
		 this->m_document->attributions) {
		Dictionary entry;
		entry["html"] = String::utf8(attribution.html.c_str());
		entry["show_on_screen"] = attribution.showOnScreen;
		result.push_back(entry);
	}
	return result;
}

PackedStringArray CesiumGeoJsonDocument::get_errors() const {
	return this->m_errors;
}

PackedStringArray CesiumGeoJsonDocument::get_warnings() const {
	return this->m_warnings;
}

const std::shared_ptr<CesiumVectorData::GeoJsonDocument>&
CesiumGeoJsonDocument::get_native_document() const {
	return this->m_document;
}

void CesiumGeoJsonDocument::initialize_native(
	std::shared_ptr<CesiumVectorData::GeoJsonDocument>&& document,
	const PackedStringArray& errors,
	const PackedStringArray& warnings
) {
	this->m_document = std::move(document);
	this->m_errors = errors;
	this->m_warnings = warnings;
	this->emit_changed();
}

void CesiumGeoJsonDocument::notify_object_changed() {
	this->emit_changed();
}

void CesiumGeoJsonDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_string", "geo_json", "attributions"), &CesiumGeoJsonDocument::load_from_string, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("clear"), &CesiumGeoJsonDocument::clear);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumGeoJsonDocument::is_valid);
	ClassDB::bind_method(D_METHOD("get_root_object"), &CesiumGeoJsonDocument::get_root_object);
	ClassDB::bind_method(D_METHOD("get_objects"), &CesiumGeoJsonDocument::get_objects);
	ClassDB::bind_method(D_METHOD("get_statistics"), &CesiumGeoJsonDocument::get_statistics);
	ClassDB::bind_method(D_METHOD("get_attributions"), &CesiumGeoJsonDocument::get_attributions);
	ClassDB::bind_method(D_METHOD("get_errors"), &CesiumGeoJsonDocument::get_errors);
	ClassDB::bind_method(D_METHOD("get_warnings"), &CesiumGeoJsonDocument::get_warnings);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_valid");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "errors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_errors");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "warnings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_warnings");
}
