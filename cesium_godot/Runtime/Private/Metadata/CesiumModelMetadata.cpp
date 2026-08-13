#include "Runtime/Public/Metadata/CesiumModelMetadata.h"

#include <algorithm>

void CesiumModelMetadata::add_enum(
	const Ref<CesiumMetadataEnum>& metadataEnum
) {
	if (metadataEnum.is_valid()) {
		this->m_enums.push_back(metadataEnum);
	}
}

Array CesiumModelMetadata::get_enums() const {
	return this->m_enums.duplicate(true);
}

int32_t CesiumModelMetadata::get_enum_count() const {
	return this->m_enums.size();
}

Ref<CesiumMetadataEnum> CesiumModelMetadata::get_enum(int32_t index) const {
	if (index < 0 || index >= this->m_enums.size()) {
		return Ref<CesiumMetadataEnum>();
	}
	return this->m_enums[index];
}

Ref<CesiumMetadataEnum> CesiumModelMetadata::find_enum(
	const String& enumId
) const {
	for (int32_t index = 0; index < this->m_enums.size(); ++index) {
		Ref<CesiumMetadataEnum> metadataEnum = this->m_enums[index];
		if (metadataEnum.is_valid() && metadataEnum->get_enum_id() == enumId) {
			return metadataEnum;
		}
	}
	return Ref<CesiumMetadataEnum>();
}

void CesiumModelMetadata::add_property_table(
	const Ref<CesiumPropertyTable>& table
) {
	if (table.is_valid()) {
		this->m_propertyTables.push_back(table);
	}
}

Array CesiumModelMetadata::get_property_tables() const {
	return this->m_propertyTables.duplicate(true);
}

int32_t CesiumModelMetadata::get_property_table_count() const {
	return this->m_propertyTables.size();
}

Ref<CesiumPropertyTable> CesiumModelMetadata::get_property_table(
	int32_t index
) const {
	if (index < 0 || index >= this->m_propertyTables.size()) {
		return Ref<CesiumPropertyTable>();
	}
	return this->m_propertyTables[index];
}

void CesiumModelMetadata::add_property_texture(
	const Ref<CesiumPropertyTexture>& texture
) {
	if (texture.is_valid()) {
		this->m_propertyTextures.push_back(texture);
	}
}

Array CesiumModelMetadata::get_property_textures() const {
	return this->m_propertyTextures.duplicate(true);
}

int32_t CesiumModelMetadata::get_property_texture_count() const {
	return this->m_propertyTextures.size();
}

Ref<CesiumPropertyTexture> CesiumModelMetadata::get_property_texture(
	int32_t index
) const {
	if (index < 0 || index >= this->m_propertyTextures.size()) {
		return Ref<CesiumPropertyTexture>();
	}
	return this->m_propertyTextures[index];
}

int64_t CesiumModelMetadata::get_property_texture_bytes() const {
	return this->m_propertyTextureBytes;
}

void CesiumModelMetadata::set_property_texture_bytes(int64_t bytes) {
	this->m_propertyTextureBytes = std::max<int64_t>(0, bytes);
}

int64_t CesiumModelMetadata::get_retained_memory_bytes() const {
	return this->m_retainedMemoryBytes;
}

void CesiumModelMetadata::set_retained_memory_bytes(int64_t bytes) {
	this->m_retainedMemoryBytes = std::max<int64_t>(0, bytes);
}

void CesiumModelMetadata::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_enums"), &CesiumModelMetadata::get_enums);
	ClassDB::bind_method(D_METHOD("get_enum_count"), &CesiumModelMetadata::get_enum_count);
	ClassDB::bind_method(D_METHOD("get_enum", "index"), &CesiumModelMetadata::get_enum);
	ClassDB::bind_method(D_METHOD("find_enum", "enum_id"), &CesiumModelMetadata::find_enum);
	ClassDB::bind_method(D_METHOD("get_property_tables"), &CesiumModelMetadata::get_property_tables);
	ClassDB::bind_method(D_METHOD("get_property_table_count"), &CesiumModelMetadata::get_property_table_count);
	ClassDB::bind_method(D_METHOD("get_property_table", "index"), &CesiumModelMetadata::get_property_table);
	ClassDB::bind_method(D_METHOD("get_property_textures"), &CesiumModelMetadata::get_property_textures);
	ClassDB::bind_method(D_METHOD("get_property_texture_count"), &CesiumModelMetadata::get_property_texture_count);
	ClassDB::bind_method(D_METHOD("get_property_texture", "index"), &CesiumModelMetadata::get_property_texture);
	ClassDB::bind_method(D_METHOD("get_property_texture_bytes"), &CesiumModelMetadata::get_property_texture_bytes);
	ClassDB::bind_method(D_METHOD("get_retained_memory_bytes"), &CesiumModelMetadata::get_retained_memory_bytes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "enums", PROPERTY_HINT_ARRAY_TYPE, "CesiumMetadataEnum", PROPERTY_USAGE_NONE), "", "get_enums");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "property_tables", PROPERTY_HINT_ARRAY_TYPE, "CesiumPropertyTable", PROPERTY_USAGE_NONE), "", "get_property_tables");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "property_textures", PROPERTY_HINT_ARRAY_TYPE, "CesiumPropertyTexture", PROPERTY_USAGE_NONE), "", "get_property_textures");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "property_texture_bytes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_texture_bytes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "retained_memory_bytes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_retained_memory_bytes");
}
