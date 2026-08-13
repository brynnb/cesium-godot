#ifndef CESIUM_MODEL_METADATA_H
#define CESIUM_MODEL_METADATA_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumModelMetadata.h.
 */

#include "Runtime/Public/Metadata/CesiumPropertyTable.h"
#include "Runtime/Public/Metadata/CesiumMetadataEnum.h"
#include "Runtime/Public/Metadata/CesiumPropertyTexture.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
using namespace godot;
#endif

/** Model-level EXT_structural_metadata exposed to Godot scripts. */
class CesiumModelMetadata : public RefCounted {
	GDCLASS(CesiumModelMetadata, RefCounted)

public:
	void add_enum(const Ref<CesiumMetadataEnum>& metadataEnum);
	Array get_enums() const;
	int32_t get_enum_count() const;
	Ref<CesiumMetadataEnum> get_enum(int32_t index) const;
	Ref<CesiumMetadataEnum> find_enum(const String& enumId) const;
	void add_property_table(const Ref<CesiumPropertyTable>& table);
	Array get_property_tables() const;
	int32_t get_property_table_count() const;
	Ref<CesiumPropertyTable> get_property_table(int32_t index) const;
	void add_property_texture(const Ref<CesiumPropertyTexture>& texture);
	Array get_property_textures() const;
	int32_t get_property_texture_count() const;
	Ref<CesiumPropertyTexture> get_property_texture(int32_t index) const;
	int64_t get_property_texture_bytes() const;
	void set_property_texture_bytes(int64_t bytes);
	int64_t get_retained_memory_bytes() const;
	void set_retained_memory_bytes(int64_t bytes);

protected:
	static void _bind_methods();

private:
	Array m_enums;
	Array m_propertyTables;
	Array m_propertyTextures;
	int64_t m_propertyTextureBytes = 0;
	int64_t m_retainedMemoryBytes = 0;
};

#endif // CESIUM_MODEL_METADATA_H
