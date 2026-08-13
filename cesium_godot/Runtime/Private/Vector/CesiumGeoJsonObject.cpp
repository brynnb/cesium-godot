// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted from Cesium for Unreal's CesiumGeoJsonObject.cpp.

#include "Runtime/Public/Vector/CesiumGeoJsonObject.h"

#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"
#include "Runtime/Public/Vector/CesiumGeoJsonDocument.h"
#include "Runtime/Public/Vector/CesiumVectorStyle.h"

#include <CesiumGeometry/AxisAlignedBox.h>
#include <CesiumVectorData/GeoJsonObjectTypes.h>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
PackedFloat64Array pack_positions(const std::vector<glm::dvec3>& positions) {
	PackedFloat64Array result;
	result.resize(static_cast<int64_t>(positions.size() * 3));
	int64_t output = 0;
	for (const glm::dvec3& position : positions) {
		result.set(output++, position.x);
		result.set(output++, position.y);
		result.set(output++, position.z);
	}
	return result;
}

Array pack_polygon(const std::vector<std::vector<glm::dvec3>>& rings) {
	Array result;
	for (const std::vector<glm::dvec3>& ring : rings) {
		result.push_back(pack_positions(ring));
	}
	return result;
}
} // namespace

bool CesiumGeoJsonObject::is_valid() const {
	return this->m_document != nullptr && this->m_object != nullptr;
}

int32_t CesiumGeoJsonObject::get_object_type() const {
	return this->is_valid()
		? static_cast<int32_t>(this->m_object->getType())
		: Invalid;
}

String CesiumGeoJsonObject::get_object_type_name() const {
	if (!this->is_valid()) return String("Invalid");
	const std::string_view name = CesiumVectorData::geoJsonObjectTypeToString(
		this->m_object->getType()
	);
	return String::utf8(name.data(), static_cast<int64_t>(name.size()));
}

bool CesiumGeoJsonObject::has_bounding_box() const {
	return this->is_valid() && this->m_object->getBoundingBox().has_value();
}

AABB CesiumGeoJsonObject::get_bounding_box() const {
	if (!this->has_bounding_box()) return AABB();
	const CesiumGeometry::AxisAlignedBox& box = *this->m_object->getBoundingBox();
	const Vector3 minimum(
		static_cast<real_t>(box.minimumX),
		static_cast<real_t>(box.minimumY),
		static_cast<real_t>(box.minimumZ)
	);
	const Vector3 maximum(
		static_cast<real_t>(box.maximumX),
		static_cast<real_t>(box.maximumY),
		static_cast<real_t>(box.maximumZ)
	);
	return AABB(minimum, maximum - minimum);
}

Dictionary CesiumGeoJsonObject::get_foreign_members() const {
	return this->is_valid()
		? CesiumGodotMetadataConversions::json_object_to_dictionary(
			this->m_object->getForeignMembers()
		)
		: Dictionary();
}

bool CesiumGeoJsonObject::has_style() const {
	return this->is_valid() && this->m_object->getStyle().has_value();
}

Ref<CesiumVectorStyle> CesiumGeoJsonObject::get_style() const {
	Ref<CesiumVectorStyle> result;
	if (!this->has_style()) return result;
	result.instantiate();
	result->initialize_from_native(*this->m_object->getStyle());
	return result;
}

bool CesiumGeoJsonObject::set_style(const Ref<CesiumVectorStyle>& style) {
	if (!this->is_valid() || style.is_null()) return false;
	const_cast<CesiumVectorData::GeoJsonObject*>(this->m_object)->getStyle() =
		style->to_native();
	this->notify_changed_owner();
	return true;
}

void CesiumGeoJsonObject::clear_style() {
	if (!this->is_valid() || !this->m_object->getStyle().has_value()) return;
	const_cast<CesiumVectorData::GeoJsonObject*>(this->m_object)->getStyle() =
		std::nullopt;
	this->notify_changed_owner();
}

PackedFloat64Array CesiumGeoJsonObject::get_points_exact() const {
	if (!this->is_valid()) return PackedFloat64Array();
	if (const auto* point = this->m_object->getIf<CesiumVectorData::GeoJsonPoint>()) {
		return pack_positions({point->coordinates});
	}
	if (const auto* points = this->m_object->getIf<CesiumVectorData::GeoJsonMultiPoint>()) {
		return pack_positions(points->coordinates);
	}
	return PackedFloat64Array();
}

Array CesiumGeoJsonObject::get_line_strings_exact() const {
	Array result;
	if (!this->is_valid()) return result;
	if (const auto* line = this->m_object->getIf<CesiumVectorData::GeoJsonLineString>()) {
		result.push_back(pack_positions(line->coordinates));
	} else if (const auto* lines = this->m_object->getIf<CesiumVectorData::GeoJsonMultiLineString>()) {
		for (const std::vector<glm::dvec3>& line : lines->coordinates) {
			result.push_back(pack_positions(line));
		}
	}
	return result;
}

Array CesiumGeoJsonObject::get_polygons_exact() const {
	Array result;
	if (!this->is_valid()) return result;
	if (const auto* polygon = this->m_object->getIf<CesiumVectorData::GeoJsonPolygon>()) {
		result.push_back(pack_polygon(polygon->coordinates));
	} else if (const auto* polygons = this->m_object->getIf<CesiumVectorData::GeoJsonMultiPolygon>()) {
		for (const std::vector<std::vector<glm::dvec3>>& polygon : polygons->coordinates) {
			result.push_back(pack_polygon(polygon));
		}
	}
	return result;
}

Array CesiumGeoJsonObject::get_children() const {
	Array result;
	if (!this->is_valid()) return result;
	if (const auto* collection = this->m_object->getIf<CesiumVectorData::GeoJsonGeometryCollection>()) {
		for (const CesiumVectorData::GeoJsonObject& child : collection->geometries) {
			result.push_back(this->wrap(&child));
		}
	} else if (const auto* collection = this->m_object->getIf<CesiumVectorData::GeoJsonFeatureCollection>()) {
		for (const CesiumVectorData::GeoJsonObject& child : collection->features) {
			result.push_back(this->wrap(&child));
		}
	} else if (const auto* feature = this->m_object->getIf<CesiumVectorData::GeoJsonFeature>()) {
		if (feature->geometry) result.push_back(this->wrap(feature->geometry.get()));
	}
	return result;
}

bool CesiumGeoJsonObject::is_feature() const {
	return this->is_valid() &&
		this->m_object->isType<CesiumVectorData::GeoJsonFeature>();
}

Variant CesiumGeoJsonObject::get_feature_id() const {
	if (!this->is_feature()) return Variant();
	const auto& id = this->m_object->get<CesiumVectorData::GeoJsonFeature>().id;
	if (const int64_t* number = std::get_if<int64_t>(&id)) return *number;
	if (const std::string* text = std::get_if<std::string>(&id)) {
		return String::utf8(text->c_str());
	}
	return Variant();
}

Dictionary CesiumGeoJsonObject::get_feature_properties() const {
	if (!this->is_feature()) return Dictionary();
	const auto& properties =
		this->m_object->get<CesiumVectorData::GeoJsonFeature>().properties;
	return properties
		? CesiumGodotMetadataConversions::json_object_to_dictionary(*properties)
		: Dictionary();
}

Ref<CesiumGeoJsonObject> CesiumGeoJsonObject::get_feature_geometry() const {
	if (!this->is_feature()) return Ref<CesiumGeoJsonObject>();
	const auto& geometry =
		this->m_object->get<CesiumVectorData::GeoJsonFeature>().geometry;
	return geometry ? this->wrap(geometry.get()) : Ref<CesiumGeoJsonObject>();
}

void CesiumGeoJsonObject::initialize(
	const std::shared_ptr<CesiumVectorData::GeoJsonDocument>& document,
	const CesiumVectorData::GeoJsonObject* object,
	const ObjectID& ownerDocument
) {
	this->m_document = document;
	this->m_object = object;
	this->m_ownerDocument = ownerDocument;
}

Ref<CesiumGeoJsonObject> CesiumGeoJsonObject::wrap(
	const CesiumVectorData::GeoJsonObject* object
) const {
	Ref<CesiumGeoJsonObject> result;
	if (object == nullptr || this->m_document == nullptr) return result;
	result.instantiate();
	result->initialize(this->m_document, object, this->m_ownerDocument);
	return result;
}

void CesiumGeoJsonObject::notify_changed_owner() {
	CesiumGeoJsonDocument* owner = Object::cast_to<CesiumGeoJsonDocument>(
		ObjectDB::get_instance(this->m_ownerDocument)
	);
	if (owner != nullptr) owner->notify_object_changed();
	this->emit_changed();
}

void CesiumGeoJsonObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumGeoJsonObject::is_valid);
	ClassDB::bind_method(D_METHOD("get_object_type"), &CesiumGeoJsonObject::get_object_type);
	ClassDB::bind_method(D_METHOD("get_object_type_name"), &CesiumGeoJsonObject::get_object_type_name);
	ClassDB::bind_method(D_METHOD("has_bounding_box"), &CesiumGeoJsonObject::has_bounding_box);
	ClassDB::bind_method(D_METHOD("get_bounding_box"), &CesiumGeoJsonObject::get_bounding_box);
	ClassDB::bind_method(D_METHOD("get_foreign_members"), &CesiumGeoJsonObject::get_foreign_members);
	ClassDB::bind_method(D_METHOD("has_style"), &CesiumGeoJsonObject::has_style);
	ClassDB::bind_method(D_METHOD("get_style"), &CesiumGeoJsonObject::get_style);
	ClassDB::bind_method(D_METHOD("set_style", "style"), &CesiumGeoJsonObject::set_style);
	ClassDB::bind_method(D_METHOD("clear_style"), &CesiumGeoJsonObject::clear_style);
	ClassDB::bind_method(D_METHOD("get_points_exact"), &CesiumGeoJsonObject::get_points_exact);
	ClassDB::bind_method(D_METHOD("get_line_strings_exact"), &CesiumGeoJsonObject::get_line_strings_exact);
	ClassDB::bind_method(D_METHOD("get_polygons_exact"), &CesiumGeoJsonObject::get_polygons_exact);
	ClassDB::bind_method(D_METHOD("get_children"), &CesiumGeoJsonObject::get_children);
	ClassDB::bind_method(D_METHOD("is_feature"), &CesiumGeoJsonObject::is_feature);
	ClassDB::bind_method(D_METHOD("get_feature_id"), &CesiumGeoJsonObject::get_feature_id);
	ClassDB::bind_method(D_METHOD("get_feature_properties"), &CesiumGeoJsonObject::get_feature_properties);
	ClassDB::bind_method(D_METHOD("get_feature_geometry"), &CesiumGeoJsonObject::get_feature_geometry);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "valid", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_valid");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "object_type", PROPERTY_HINT_ENUM, "Point,MultiPoint,LineString,MultiLineString,Polygon,MultiPolygon,GeometryCollection,Feature,FeatureCollection,Invalid", PROPERTY_USAGE_NONE), "", "get_object_type");
	BIND_ENUM_CONSTANT(Point);
	BIND_ENUM_CONSTANT(MultiPoint);
	BIND_ENUM_CONSTANT(LineString);
	BIND_ENUM_CONSTANT(MultiLineString);
	BIND_ENUM_CONSTANT(Polygon);
	BIND_ENUM_CONSTANT(MultiPolygon);
	BIND_ENUM_CONSTANT(GeometryCollection);
	BIND_ENUM_CONSTANT(Feature);
	BIND_ENUM_CONSTANT(FeatureCollection);
	BIND_ENUM_CONSTANT(Invalid);
}
