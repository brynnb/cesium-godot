// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumPropertyTextureProperty.h"

#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"
#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumPropertyTextureProperty::initialize(
	const std::shared_ptr<const CesiumPropertyTexturePropertySnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

int32_t CesiumPropertyTextureProperty::get_status() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->status)
		: static_cast<int32_t>(
			CesiumPropertyTexturePropertyStatus::ErrorInvalidProperty
		);
}

String CesiumPropertyTextureProperty::get_status_name() const {
	switch (static_cast<CesiumPropertyTexturePropertyStatus>(
		this->get_status()
	)) {
	case CesiumPropertyTexturePropertyStatus::Valid:
		return "valid";
	case CesiumPropertyTexturePropertyStatus::EmptyPropertyWithDefault:
		return "empty_property_with_default";
	case CesiumPropertyTexturePropertyStatus::ErrorInvalidProperty:
		return "error_invalid_property";
	case CesiumPropertyTexturePropertyStatus::ErrorInvalidPropertyData:
		return "error_invalid_property_data";
	case CesiumPropertyTexturePropertyStatus::ErrorUnsupportedProperty:
		return "error_unsupported_property";
	}
	return "error_invalid_property";
}

bool CesiumPropertyTextureProperty::is_valid() const {
	return this->m_snapshot && (
		this->m_snapshot->status ==
			CesiumPropertyTexturePropertyStatus::Valid ||
		this->m_snapshot->status ==
			CesiumPropertyTexturePropertyStatus::EmptyPropertyWithDefault
	);
}

String CesiumPropertyTextureProperty::get_property_id() const {
	return this->m_snapshot ? String(this->m_snapshot->id.c_str()) : String();
}

String CesiumPropertyTextureProperty::get_value_type() const {
	return this->m_snapshot ? String(this->m_snapshot->type.c_str()) : String();
}

String CesiumPropertyTextureProperty::get_component_type() const {
	return this->m_snapshot
		? String(this->m_snapshot->componentType.c_str())
		: String();
}

String CesiumPropertyTextureProperty::get_enum_type() const {
	return this->m_snapshot
		? String(this->m_snapshot->enumType.c_str())
		: String();
}

bool CesiumPropertyTextureProperty::get_is_array() const {
	return this->m_snapshot && this->m_snapshot->isArray;
}

bool CesiumPropertyTextureProperty::get_normalized() const {
	return this->m_snapshot && this->m_snapshot->normalized;
}

int64_t CesiumPropertyTextureProperty::get_fixed_array_count() const {
	return this->m_snapshot ? this->m_snapshot->fixedArrayCount : 0;
}

int64_t CesiumPropertyTextureProperty::get_texture_coordinate_set_index() const {
	return this->m_snapshot
		? this->m_snapshot->textureCoordinateSetIndex
		: -1;
}

Array CesiumPropertyTextureProperty::get_channels() const {
	Array result;
	if (!this->m_snapshot) {
		return result;
	}
	for (int64_t channel : this->m_snapshot->channels) {
		result.push_back(channel);
	}
	return result;
}

Dictionary CesiumPropertyTextureProperty::get_sampler_details() const {
	Dictionary result;
	if (this->m_snapshot) {
		result["wrap_s"] = this->m_snapshot->wrapS;
		result["wrap_t"] = this->m_snapshot->wrapT;
		result["filtering"] = "nearest";
	}
	return result;
}

Dictionary CesiumPropertyTextureProperty::get_texture_transform_details() const {
	Dictionary result;
	if (!this->m_snapshot || !this->m_snapshot->hasTextureTransform) {
		return result;
	}
	result["offset"] = Vector2(
		this->m_snapshot->textureOffsetX,
		this->m_snapshot->textureOffsetY
	);
	result["scale"] = Vector2(
		this->m_snapshot->textureScaleX,
		this->m_snapshot->textureScaleY
	);
	result["rotation"] = this->m_snapshot->textureRotation;
	result["texture_coordinate_set_index"] =
		this->m_snapshot->textureTransformCoordinateSetIndex;
	return result;
}

Dictionary CesiumPropertyTextureProperty::get_value_transform_details() const {
	Dictionary result;
	if (!this->m_snapshot) {
		return result;
	}
	result["offset"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->offset
	);
	result["scale"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->scale
	);
	result["minimum"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->minimum
	);
	result["maximum"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->maximum
	);
	result["no_data"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->noData
	);
	result["default"] = CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->defaultValue
	);
	return result;
}

Variant CesiumPropertyTextureProperty::get_value(
	const Vector2& textureCoordinates
) const {
	if (!this->m_snapshot || !this->m_snapshot->sampleValue) {
		return Variant();
	}
	return CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->sampleValue(
			textureCoordinates.x,
			textureCoordinates.y
		)
	).duplicate(true);
}

Variant CesiumPropertyTextureProperty::get_raw_value(
	const Vector2& textureCoordinates
) const {
	if (!this->m_snapshot || !this->m_snapshot->sampleRawValue) {
		return Variant();
	}
	return CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->sampleRawValue(
			textureCoordinates.x,
			textureCoordinates.y
		)
	).duplicate(true);
}

void CesiumPropertyTextureProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumPropertyTextureProperty::get_status);
	ClassDB::bind_method(D_METHOD("get_status_name"), &CesiumPropertyTextureProperty::get_status_name);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumPropertyTextureProperty::is_valid);
	ClassDB::bind_method(D_METHOD("get_property_id"), &CesiumPropertyTextureProperty::get_property_id);
	ClassDB::bind_method(D_METHOD("get_value_type"), &CesiumPropertyTextureProperty::get_value_type);
	ClassDB::bind_method(D_METHOD("get_component_type"), &CesiumPropertyTextureProperty::get_component_type);
	ClassDB::bind_method(D_METHOD("get_enum_type"), &CesiumPropertyTextureProperty::get_enum_type);
	ClassDB::bind_method(D_METHOD("get_is_array"), &CesiumPropertyTextureProperty::get_is_array);
	ClassDB::bind_method(D_METHOD("get_normalized"), &CesiumPropertyTextureProperty::get_normalized);
	ClassDB::bind_method(D_METHOD("get_fixed_array_count"), &CesiumPropertyTextureProperty::get_fixed_array_count);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_set_index"), &CesiumPropertyTextureProperty::get_texture_coordinate_set_index);
	ClassDB::bind_method(D_METHOD("get_channels"), &CesiumPropertyTextureProperty::get_channels);
	ClassDB::bind_method(D_METHOD("get_sampler_details"), &CesiumPropertyTextureProperty::get_sampler_details);
	ClassDB::bind_method(D_METHOD("get_texture_transform_details"), &CesiumPropertyTextureProperty::get_texture_transform_details);
	ClassDB::bind_method(D_METHOD("get_value_transform_details"), &CesiumPropertyTextureProperty::get_value_transform_details);
	ClassDB::bind_method(D_METHOD("get_value", "texture_coordinates"), &CesiumPropertyTextureProperty::get_value);
	ClassDB::bind_method(D_METHOD("get_raw_value", "texture_coordinates"), &CesiumPropertyTextureProperty::get_raw_value);

	BIND_ENUM_CONSTANT(Valid);
	BIND_ENUM_CONSTANT(EmptyPropertyWithDefault);
	BIND_ENUM_CONSTANT(ErrorInvalidProperty);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyData);
	BIND_ENUM_CONSTANT(ErrorUnsupportedProperty);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_value_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "component_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_component_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "enum_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_enum_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_array", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_is_array");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalized", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_normalized");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_array_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_fixed_array_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_coordinate_set_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_coordinate_set_index");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "channels", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_channels");
}
