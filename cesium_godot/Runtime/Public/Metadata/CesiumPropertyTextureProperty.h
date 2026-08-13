// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PROPERTY_TEXTURE_PROPERTY_H
#define CESIUM_PROPERTY_TEXTURE_PROPERTY_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPropertyTextureProperty.h.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/vector2.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/vector2.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumPropertyTexturePropertySnapshot;

class CesiumPropertyTextureProperty : public RefCounted {
	GDCLASS(CesiumPropertyTextureProperty, RefCounted)

public:
	enum Status {
		Valid = 0,
		EmptyPropertyWithDefault = 1,
		ErrorInvalidProperty = 2,
		ErrorInvalidPropertyData = 3,
		ErrorUnsupportedProperty = 4,
	};

	void initialize(
		const std::shared_ptr<const CesiumPropertyTexturePropertySnapshot>&
			snapshot
	);
	int32_t get_status() const;
	String get_status_name() const;
	bool is_valid() const;
	String get_property_id() const;
	String get_value_type() const;
	String get_component_type() const;
	String get_enum_type() const;
	bool get_is_array() const;
	bool get_normalized() const;
	int64_t get_fixed_array_count() const;
	int64_t get_texture_coordinate_set_index() const;
	Array get_channels() const;
	Dictionary get_sampler_details() const;
	Dictionary get_texture_transform_details() const;
	Dictionary get_value_transform_details() const;
	Variant get_value(const Vector2& textureCoordinates) const;
	Variant get_raw_value(const Vector2& textureCoordinates) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumPropertyTexturePropertySnapshot> m_snapshot;
};

VARIANT_ENUM_CAST(CesiumPropertyTextureProperty::Status);

#endif // CESIUM_PROPERTY_TEXTURE_PROPERTY_H
