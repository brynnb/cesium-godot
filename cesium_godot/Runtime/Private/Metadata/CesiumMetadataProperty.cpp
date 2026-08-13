#include "Runtime/Public/Metadata/CesiumMetadataProperty.h"

#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"
#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumMetadataProperty::initialize(
	const std::shared_ptr<const CesiumMetadataPropertySnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

int32_t CesiumMetadataProperty::get_status() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->status)
		: static_cast<int32_t>(
			CesiumMetadataPropertyStatus::ErrorInvalidProperty
		);
}

String CesiumMetadataProperty::get_status_name() const {
	switch (static_cast<CesiumMetadataPropertyStatus>(this->get_status())) {
	case CesiumMetadataPropertyStatus::Valid:
		return "valid";
	case CesiumMetadataPropertyStatus::EmptyPropertyWithDefault:
		return "empty_property_with_default";
	case CesiumMetadataPropertyStatus::ErrorInvalidProperty:
		return "error_invalid_property";
	case CesiumMetadataPropertyStatus::ErrorInvalidPropertyData:
		return "error_invalid_property_data";
	}
	return "error_invalid_property";
}

bool CesiumMetadataProperty::is_valid() const {
	return this->m_snapshot && (
		this->m_snapshot->status == CesiumMetadataPropertyStatus::Valid ||
		this->m_snapshot->status ==
			CesiumMetadataPropertyStatus::EmptyPropertyWithDefault
	);
}

String CesiumMetadataProperty::get_property_id() const {
	return this->m_snapshot ? String(this->m_snapshot->id.c_str()) : String();
}

String CesiumMetadataProperty::get_value_type() const {
	return this->m_snapshot ? String(this->m_snapshot->type.c_str()) : String();
}

String CesiumMetadataProperty::get_component_type() const {
	return this->m_snapshot
		? String(this->m_snapshot->componentType.c_str())
		: String();
}

String CesiumMetadataProperty::get_enum_type() const {
	return this->m_snapshot
		? String(this->m_snapshot->enumType.c_str())
		: String();
}

bool CesiumMetadataProperty::get_is_array() const {
	return this->m_snapshot && this->m_snapshot->isArray;
}

bool CesiumMetadataProperty::get_normalized() const {
	return this->m_snapshot && this->m_snapshot->normalized;
}

int64_t CesiumMetadataProperty::get_fixed_array_count() const {
	return this->m_snapshot ? this->m_snapshot->fixedArrayCount : 0;
}

int64_t CesiumMetadataProperty::get_size() const {
	return this->m_snapshot
		? this->m_snapshot->size
		: 0;
}

Variant CesiumMetadataProperty::get_value(int64_t featureId) const {
	if (!this->m_snapshot || featureId < 0) {
		return Variant();
	}
	if (
		this->m_snapshot->status ==
			CesiumMetadataPropertyStatus::EmptyPropertyWithDefault
	) {
		return CesiumGodotMetadataConversions::json_value_to_variant(
			this->m_snapshot->defaultValue
		).duplicate(true);
	}
	if (
		featureId >= static_cast<int64_t>(this->m_snapshot->values.size())
	) {
		return Variant();
	}
	return CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->values[static_cast<size_t>(featureId)]
	).duplicate(true);
}

Variant CesiumMetadataProperty::get_raw_value(int64_t featureId) const {
	if (
		!this->m_snapshot || featureId < 0 ||
		featureId >= static_cast<int64_t>(this->m_snapshot->rawValues.size())
	) {
		return Variant();
	}
	return CesiumGodotMetadataConversions::json_value_to_variant(
		this->m_snapshot->rawValues[static_cast<size_t>(featureId)]
	).duplicate(true);
}

Array CesiumMetadataProperty::get_values() const {
	Array result;
	if (!this->m_snapshot) {
		return result;
	}
	result.resize(this->get_size());
	for (int64_t index = 0; index < result.size(); ++index) {
		result[index] = this->get_value(index);
	}
	return result;
}

Array CesiumMetadataProperty::get_raw_values() const {
	Array result;
	if (!this->m_snapshot) {
		return result;
	}
	result.resize(static_cast<int64_t>(this->m_snapshot->rawValues.size()));
	for (int64_t index = 0; index < result.size(); ++index) {
		result[index] = this->get_raw_value(index);
	}
	return result;
}

Dictionary CesiumMetadataProperty::get_value_transform_details() const {
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

void CesiumMetadataProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumMetadataProperty::get_status);
	ClassDB::bind_method(D_METHOD("get_status_name"), &CesiumMetadataProperty::get_status_name);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumMetadataProperty::is_valid);
	ClassDB::bind_method(D_METHOD("get_property_id"), &CesiumMetadataProperty::get_property_id);
	ClassDB::bind_method(D_METHOD("get_value_type"), &CesiumMetadataProperty::get_value_type);
	ClassDB::bind_method(D_METHOD("get_component_type"), &CesiumMetadataProperty::get_component_type);
	ClassDB::bind_method(D_METHOD("get_enum_type"), &CesiumMetadataProperty::get_enum_type);
	ClassDB::bind_method(D_METHOD("get_is_array"), &CesiumMetadataProperty::get_is_array);
	ClassDB::bind_method(D_METHOD("get_normalized"), &CesiumMetadataProperty::get_normalized);
	ClassDB::bind_method(D_METHOD("get_fixed_array_count"), &CesiumMetadataProperty::get_fixed_array_count);
	ClassDB::bind_method(D_METHOD("get_size"), &CesiumMetadataProperty::get_size);
	ClassDB::bind_method(D_METHOD("get_value", "feature_id"), &CesiumMetadataProperty::get_value);
	ClassDB::bind_method(D_METHOD("get_raw_value", "feature_id"), &CesiumMetadataProperty::get_raw_value);
	ClassDB::bind_method(D_METHOD("get_values"), &CesiumMetadataProperty::get_values);
	ClassDB::bind_method(D_METHOD("get_raw_values"), &CesiumMetadataProperty::get_raw_values);
	ClassDB::bind_method(D_METHOD("get_value_transform_details"), &CesiumMetadataProperty::get_value_transform_details);

	BIND_ENUM_CONSTANT(Valid);
	BIND_ENUM_CONSTANT(EmptyPropertyWithDefault);
	BIND_ENUM_CONSTANT(ErrorInvalidProperty);
	BIND_ENUM_CONSTANT(ErrorInvalidPropertyData);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_value_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "component_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_component_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "enum_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_enum_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_array", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_is_array");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalized", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_normalized");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fixed_array_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_fixed_array_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_size");
}
