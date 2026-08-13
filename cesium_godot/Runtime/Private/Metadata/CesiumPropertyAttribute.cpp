// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumPropertyAttribute.h"

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumPropertyAttribute::initialize(
	const std::shared_ptr<const CesiumPropertyAttributeSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

void CesiumPropertyAttribute::add_property(
	const Ref<CesiumPropertyAttributeProperty>& property
) {
	if (property.is_valid()) {
		this->m_properties.push_back(property);
	}
}

int32_t CesiumPropertyAttribute::get_status() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->status)
		: static_cast<int32_t>(
			CesiumPropertyAttributeStatus::ErrorInvalidPropertyAttribute
		);
}

String CesiumPropertyAttribute::get_status_name() const {
	switch (static_cast<CesiumPropertyAttributeStatus>(this->get_status())) {
	case CesiumPropertyAttributeStatus::Valid:
		return "valid";
	case CesiumPropertyAttributeStatus::ErrorInvalidPropertyAttribute:
		return "error_invalid_property_attribute";
	case CesiumPropertyAttributeStatus::ErrorInvalidPropertyAttributeClass:
		return "error_invalid_property_attribute_class";
	}
	return "error_invalid_property_attribute";
}

bool CesiumPropertyAttribute::is_valid() const {
	return this->m_snapshot &&
		this->m_snapshot->status == CesiumPropertyAttributeStatus::Valid;
}

String CesiumPropertyAttribute::get_attribute_name() const {
	return this->m_snapshot
		? String(this->m_snapshot->name.c_str())
		: String();
}

String CesiumPropertyAttribute::get_class_name() const {
	return this->m_snapshot
		? String(this->m_snapshot->className.c_str())
		: String();
}

int64_t CesiumPropertyAttribute::get_element_count() const {
	return this->m_snapshot ? this->m_snapshot->elementCount : 0;
}

Array CesiumPropertyAttribute::get_properties() const {
	return this->m_properties.duplicate(true);
}

Array CesiumPropertyAttribute::get_property_names() const {
	Array result;
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyAttributeProperty> property = this->m_properties[index];
		if (property.is_valid()) {
			result.push_back(property->get_property_id());
		}
	}
	return result;
}

Ref<CesiumPropertyAttributeProperty> CesiumPropertyAttribute::find_property(
	const String& propertyId
) const {
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyAttributeProperty> property = this->m_properties[index];
		if (
			property.is_valid() && property->get_property_id() == propertyId
		) {
			return property;
		}
	}
	return Ref<CesiumPropertyAttributeProperty>();
}

Dictionary CesiumPropertyAttribute::get_metadata_values_at_index(
	int64_t vertexIndex
) const {
	Dictionary result;
	if (vertexIndex < 0 || vertexIndex >= this->get_element_count()) {
		return result;
	}
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyAttributeProperty> property = this->m_properties[index];
		if (property.is_valid() && property->is_valid()) {
			result[property->get_property_id()] = property->get_value(vertexIndex);
		}
	}
	return result;
}

void CesiumPropertyAttribute::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumPropertyAttribute::get_status);
	ClassDB::bind_method(D_METHOD("get_status_name"), &CesiumPropertyAttribute::get_status_name);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumPropertyAttribute::is_valid);
	ClassDB::bind_method(D_METHOD("get_attribute_name"), &CesiumPropertyAttribute::get_attribute_name);
	ClassDB::bind_method(D_METHOD("get_class_name"), &CesiumPropertyAttribute::get_class_name);
	ClassDB::bind_method(D_METHOD("get_element_count"), &CesiumPropertyAttribute::get_element_count);
	ClassDB::bind_method(D_METHOD("get_properties"), &CesiumPropertyAttribute::get_properties);
	ClassDB::bind_method(D_METHOD("get_property_names"), &CesiumPropertyAttribute::get_property_names);
	ClassDB::bind_method(D_METHOD("find_property", "property_id"), &CesiumPropertyAttribute::find_property);
	ClassDB::bind_method(D_METHOD("get_metadata_values_at_index", "vertex_index"), &CesiumPropertyAttribute::get_metadata_values_at_index);

	BIND_ENUM_CONSTANT(Valid);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyAttribute);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyAttributeClass);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "attribute_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_attribute_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_class_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "element_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_element_count");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "properties", PROPERTY_HINT_ARRAY_TYPE, "CesiumPropertyAttributeProperty", PROPERTY_USAGE_NONE), "", "get_properties");
}
