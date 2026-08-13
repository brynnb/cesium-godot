#ifndef CESIUM_PROPERTY_TABLE_H
#define CESIUM_PROPERTY_TABLE_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyTable.h.
 */

#include "Runtime/Public/Metadata/CesiumMetadataProperty.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"
using namespace godot;
#endif

/** A Godot-native, immutable-by-API property table resource. */
struct CesiumPropertyTableSnapshot;

class CesiumPropertyTable : public RefCounted {
	GDCLASS(CesiumPropertyTable, RefCounted)

public:
	void initialize(const CesiumPropertyTableSnapshot& snapshot);
	void add_property(const Ref<CesiumMetadataProperty>& property);

	bool is_valid() const;
	String get_table_name() const;
	String get_class_name() const;
	int64_t get_count() const;
	Dictionary get_properties() const;
	Array get_property_names() const;
	Ref<CesiumMetadataProperty> find_property(const String& propertyId) const;
	Dictionary get_metadata_values_for_feature(int64_t featureId) const;

protected:
	static void _bind_methods();

private:
	bool m_valid = false;
	String m_name;
	String m_className;
	int64_t m_count = 0;
	Dictionary m_properties;
};

#endif // CESIUM_PROPERTY_TABLE_H
