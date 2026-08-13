// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_FEATURE_ID_SET_H
#define CESIUM_FEATURE_ID_SET_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumFeatureIdSet.h,
 * CesiumFeatureIdAttribute.h, and CesiumFeatureIdTexture.h.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/math/vector2.h"
#include "core/object/ref_counted.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector2.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumFeatureIdSetSnapshot;

class CesiumFeatureIdSet : public RefCounted {
	GDCLASS(CesiumFeatureIdSet, RefCounted)

public:
	enum Type {
		None = 0,
		Attribute = 1,
		Texture = 2,
		Implicit = 3,
		Instance = 4,
		InstanceImplicit = 5,
	};

	enum Status {
		Valid = 0,
		ErrorInvalidDefinition = 1,
		ErrorInvalidAccessor = 2,
		ErrorInvalidTexture = 3,
	};

	void initialize(
		const std::shared_ptr<const CesiumFeatureIdSetSnapshot>& snapshot
	);

	int32_t get_feature_id_set_type() const;
	String get_feature_id_set_type_name() const;
	int32_t get_status() const;
	String get_status_name() const;
	bool is_valid() const;
	int64_t get_feature_count() const;
	int64_t get_null_feature_id() const;
	int32_t get_property_table_index() const;
	String get_label() const;
	int64_t get_attribute_index() const;
	int64_t get_texture_coordinate_set_index() const;
	int64_t get_vertex_count() const;
	int64_t get_instance_count() const;
	int64_t get_feature_id_for_vertex(int64_t vertexIndex) const;
	int64_t get_feature_id_for_instance(int64_t instanceIndex) const;
	int64_t get_feature_id_for_texture_coordinate(
		const Vector2& textureCoordinate
	) const;
	bool is_null_feature_id(int64_t featureId) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumFeatureIdSetSnapshot> m_snapshot;
};

VARIANT_ENUM_CAST(CesiumFeatureIdSet::Type);
VARIANT_ENUM_CAST(CesiumFeatureIdSet::Status);

#endif // CESIUM_FEATURE_ID_SET_H
