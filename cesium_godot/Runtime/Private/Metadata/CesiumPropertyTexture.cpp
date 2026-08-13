// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumPropertyTexture.h"

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumPropertyTexture::initialize(
	const std::shared_ptr<const CesiumPropertyTextureSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

void CesiumPropertyTexture::add_property(
	const Ref<CesiumPropertyTextureProperty>& property
) {
	if (property.is_valid()) {
		this->m_properties.push_back(property);
	}
}

int32_t CesiumPropertyTexture::get_status() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->status)
		: static_cast<int32_t>(
			CesiumPropertyTextureStatus::ErrorInvalidPropertyTexture
		);
}

String CesiumPropertyTexture::get_status_name() const {
	switch (static_cast<CesiumPropertyTextureStatus>(this->get_status())) {
	case CesiumPropertyTextureStatus::Valid:
		return "valid";
	case CesiumPropertyTextureStatus::ErrorInvalidPropertyTexture:
		return "error_invalid_property_texture";
	case CesiumPropertyTextureStatus::ErrorInvalidPropertyTextureClass:
		return "error_invalid_property_texture_class";
	}
	return "error_invalid_property_texture";
}

bool CesiumPropertyTexture::is_valid() const {
	return this->m_snapshot &&
		this->m_snapshot->status == CesiumPropertyTextureStatus::Valid;
}

String CesiumPropertyTexture::get_texture_name() const {
	return this->m_snapshot
		? String(this->m_snapshot->name.c_str())
		: String();
}

String CesiumPropertyTexture::get_class_name() const {
	return this->m_snapshot
		? String(this->m_snapshot->className.c_str())
		: String();
}

Array CesiumPropertyTexture::get_properties() const {
	return this->m_properties.duplicate(true);
}

Array CesiumPropertyTexture::get_property_names() const {
	Array result;
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyTextureProperty> property = this->m_properties[index];
		if (property.is_valid()) {
			result.push_back(property->get_property_id());
		}
	}
	return result;
}

Ref<CesiumPropertyTextureProperty> CesiumPropertyTexture::find_property(
	const String& propertyId
) const {
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyTextureProperty> property = this->m_properties[index];
		if (
			property.is_valid() && property->get_property_id() == propertyId
		) {
			return property;
		}
	}
	return Ref<CesiumPropertyTextureProperty>();
}

Dictionary CesiumPropertyTexture::get_metadata_values_for_uv(
	const Vector2& textureCoordinates
) const {
	Dictionary result;
	for (int32_t index = 0; index < this->m_properties.size(); ++index) {
		Ref<CesiumPropertyTextureProperty> property = this->m_properties[index];
		if (property.is_valid() && property->is_valid()) {
			result[property->get_property_id()] = property->get_value(
				textureCoordinates
			);
		}
	}
	return result;
}

void CesiumPropertyTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumPropertyTexture::get_status);
	ClassDB::bind_method(D_METHOD("get_status_name"), &CesiumPropertyTexture::get_status_name);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumPropertyTexture::is_valid);
	ClassDB::bind_method(D_METHOD("get_texture_name"), &CesiumPropertyTexture::get_texture_name);
	ClassDB::bind_method(D_METHOD("get_class_name"), &CesiumPropertyTexture::get_class_name);
	ClassDB::bind_method(D_METHOD("get_properties"), &CesiumPropertyTexture::get_properties);
	ClassDB::bind_method(D_METHOD("get_property_names"), &CesiumPropertyTexture::get_property_names);
	ClassDB::bind_method(D_METHOD("find_property", "property_id"), &CesiumPropertyTexture::find_property);
	ClassDB::bind_method(D_METHOD("get_metadata_values_for_uv", "texture_coordinates"), &CesiumPropertyTexture::get_metadata_values_for_uv);

	BIND_ENUM_CONSTANT(Valid);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyTexture);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyTextureClass);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_class_name");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "properties", PROPERTY_HINT_ARRAY_TYPE, "CesiumPropertyTextureProperty", PROPERTY_USAGE_NONE), "", "get_properties");
}
