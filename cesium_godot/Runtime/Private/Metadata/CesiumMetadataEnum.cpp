// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumMetadataEnum.h"

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumMetadataEnum::initialize(
	const std::shared_ptr<const CesiumMetadataEnumSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

String CesiumMetadataEnum::get_enum_id() const {
	return this->m_snapshot ? String(this->m_snapshot->id.c_str()) : String();
}

String CesiumMetadataEnum::get_enum_name() const {
	return this->m_snapshot ? String(this->m_snapshot->name.c_str()) : String();
}

String CesiumMetadataEnum::get_description() const {
	return this->m_snapshot
		? String(this->m_snapshot->description.c_str())
		: String();
}

String CesiumMetadataEnum::get_value_type() const {
	return this->m_snapshot
		? String(this->m_snapshot->valueType.c_str())
		: String();
}

int32_t CesiumMetadataEnum::get_value_count() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->values.size())
		: 0;
}

Array CesiumMetadataEnum::get_values() const {
	Array result;
	if (!this->m_snapshot) {
		return result;
	}
	for (const CesiumMetadataEnumValueSnapshot& value : this->m_snapshot->values) {
		Dictionary details;
		details["value"] = value.value;
		details["name"] = String(value.name.c_str());
		details["description"] = String(value.description.c_str());
		result.push_back(details);
	}
	return result;
}

bool CesiumMetadataEnum::has_value(int64_t value) const {
	if (!this->m_snapshot) {
		return false;
	}
	for (const CesiumMetadataEnumValueSnapshot& candidate : this->m_snapshot->values) {
		if (candidate.value == value) {
			return true;
		}
	}
	return false;
}

String CesiumMetadataEnum::get_name_for_value(int64_t value) const {
	if (!this->m_snapshot) {
		return String();
	}
	for (const CesiumMetadataEnumValueSnapshot& candidate : this->m_snapshot->values) {
		if (candidate.value == value) {
			return String(candidate.name.c_str());
		}
	}
	return String();
}

Dictionary CesiumMetadataEnum::get_value_details(int64_t value) const {
	if (!this->m_snapshot) {
		return Dictionary();
	}
	for (const CesiumMetadataEnumValueSnapshot& candidate : this->m_snapshot->values) {
		if (candidate.value == value) {
			Dictionary result;
			result["value"] = candidate.value;
			result["name"] = String(candidate.name.c_str());
			result["description"] = String(candidate.description.c_str());
			return result;
		}
	}
	return Dictionary();
}

void CesiumMetadataEnum::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_enum_id"), &CesiumMetadataEnum::get_enum_id);
	ClassDB::bind_method(D_METHOD("get_enum_name"), &CesiumMetadataEnum::get_enum_name);
	ClassDB::bind_method(D_METHOD("get_description"), &CesiumMetadataEnum::get_description);
	ClassDB::bind_method(D_METHOD("get_value_type"), &CesiumMetadataEnum::get_value_type);
	ClassDB::bind_method(D_METHOD("get_value_count"), &CesiumMetadataEnum::get_value_count);
	ClassDB::bind_method(D_METHOD("get_values"), &CesiumMetadataEnum::get_values);
	ClassDB::bind_method(D_METHOD("has_value", "value"), &CesiumMetadataEnum::has_value);
	ClassDB::bind_method(D_METHOD("get_name_for_value", "value"), &CesiumMetadataEnum::get_name_for_value);
	ClassDB::bind_method(D_METHOD("get_value_details", "value"), &CesiumMetadataEnum::get_value_details);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "enum_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_enum_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "enum_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_enum_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_value_type");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_values");
}
