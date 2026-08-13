#include "Runtime/Public/Metadata/CesiumPropertyTable.h"

#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"

void CesiumPropertyTable::initialize(
	const CesiumPropertyTableSnapshot& snapshot
) {
	this->m_valid = snapshot.valid;
	this->m_name = String(snapshot.name.c_str());
	this->m_className = String(snapshot.className.c_str());
	this->m_count = snapshot.count;
}

void CesiumPropertyTable::add_property(
	const Ref<CesiumMetadataProperty>& property
) {
	if (property.is_null()) {
		return;
	}
	this->m_properties[property->get_property_id()] = property;
}

bool CesiumPropertyTable::is_valid() const {
	return this->m_valid;
}

String CesiumPropertyTable::get_table_name() const {
	return this->m_name;
}

String CesiumPropertyTable::get_class_name() const {
	return this->m_className;
}

int64_t CesiumPropertyTable::get_count() const {
	return this->m_valid ? this->m_count : 0;
}

Dictionary CesiumPropertyTable::get_properties() const {
	return this->m_properties.duplicate(true);
}

Array CesiumPropertyTable::get_property_names() const {
	return this->m_properties.keys();
}

Ref<CesiumMetadataProperty> CesiumPropertyTable::find_property(
	const String& propertyId
) const {
	if (!this->m_properties.has(propertyId)) {
		return Ref<CesiumMetadataProperty>();
	}
	return this->m_properties[propertyId];
}

Dictionary CesiumPropertyTable::get_metadata_values_for_feature(
	int64_t featureId
) const {
	Dictionary result;
	if (!this->m_valid || featureId < 0 || featureId >= this->m_count) {
		return result;
	}
	Array keys = this->m_properties.keys();
	for (int64_t index = 0; index < keys.size(); ++index) {
		const String propertyId = keys[index];
		Ref<CesiumMetadataProperty> property =
			this->m_properties[propertyId];
		if (property.is_valid() && property->is_valid()) {
			result[propertyId] = property->get_value(featureId);
		}
	}
	return result;
}

void CesiumPropertyTable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumPropertyTable::is_valid);
	ClassDB::bind_method(D_METHOD("get_table_name"), &CesiumPropertyTable::get_table_name);
	ClassDB::bind_method(D_METHOD("get_class_name"), &CesiumPropertyTable::get_class_name);
	ClassDB::bind_method(D_METHOD("get_count"), &CesiumPropertyTable::get_count);
	ClassDB::bind_method(D_METHOD("get_properties"), &CesiumPropertyTable::get_properties);
	ClassDB::bind_method(D_METHOD("get_property_names"), &CesiumPropertyTable::get_property_names);
	ClassDB::bind_method(D_METHOD("find_property", "property_id"), &CesiumPropertyTable::find_property);
	ClassDB::bind_method(D_METHOD("get_metadata_values_for_feature", "feature_id"), &CesiumPropertyTable::get_metadata_values_for_feature);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "table_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_table_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_class_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_count");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "properties", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_properties");
}
