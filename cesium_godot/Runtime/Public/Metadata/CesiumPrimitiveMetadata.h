// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PRIMITIVE_METADATA_H
#define CESIUM_PRIMITIVE_METADATA_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPrimitiveMetadata.h.
 */

#include "Runtime/Public/Metadata/CesiumPropertyAttribute.h"

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

class CesiumModelMetadata;
struct CesiumPrimitiveMetadataSnapshot;

class CesiumPrimitiveMetadata : public RefCounted {
	GDCLASS(CesiumPrimitiveMetadata, RefCounted)

public:
	void initialize(
		const std::shared_ptr<const CesiumPrimitiveMetadataSnapshot>& snapshot
	);
	void add_property_attribute(const Ref<CesiumPropertyAttribute>& attribute);
	Array get_property_texture_indices() const;
	Array get_property_attribute_indices() const;
	Array get_property_attributes() const;
	int32_t get_property_attribute_count() const;
	Ref<CesiumPropertyAttribute> get_property_attribute(int32_t index) const;
	Variant find_texture_coordinate_from_hit(
		int64_t faceIndex,
		const Vector3& localPosition,
		int64_t textureCoordinateSetIndex
	) const;
	Dictionary get_property_texture_values_from_hit(
		const Ref<CesiumModelMetadata>& modelMetadata,
		int32_t propertyTextureIndex,
		int64_t faceIndex,
		const Vector3& localPosition
	) const;
	int64_t get_retained_memory_bytes() const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumPrimitiveMetadataSnapshot> m_snapshot;
	Array m_propertyAttributes;
};

#endif // CESIUM_PRIMITIVE_METADATA_H
