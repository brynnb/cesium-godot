// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PROPERTY_ATTRIBUTE_H
#define CESIUM_PROPERTY_ATTRIBUTE_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyAttribute.h.
 */

#include "Runtime/Public/Metadata/CesiumPropertyAttributeProperty.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumPropertyAttributeSnapshot;

class CesiumPropertyAttribute : public RefCounted {
	GDCLASS(CesiumPropertyAttribute, RefCounted)

public:
	enum Status {
		Valid = 0,
		ErrorInvalidPropertyAttribute = 1,
		ErrorInvalidPropertyAttributeClass = 2,
	};

	void initialize(
		const std::shared_ptr<const CesiumPropertyAttributeSnapshot>& snapshot
	);
	void add_property(const Ref<CesiumPropertyAttributeProperty>& property);
	int32_t get_status() const;
	String get_status_name() const;
	bool is_valid() const;
	String get_attribute_name() const;
	String get_class_name() const;
	int64_t get_element_count() const;
	Array get_properties() const;
	Array get_property_names() const;
	Ref<CesiumPropertyAttributeProperty> find_property(
		const String& propertyId
	) const;
	Dictionary get_metadata_values_at_index(int64_t vertexIndex) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumPropertyAttributeSnapshot> m_snapshot;
	Array m_properties;
};

VARIANT_ENUM_CAST(CesiumPropertyAttribute::Status);

#endif // CESIUM_PROPERTY_ATTRIBUTE_H
