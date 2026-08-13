// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_PRIMITIVE_FEATURES_H
#define CESIUM_PRIMITIVE_FEATURES_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumPrimitiveFeatures.h and
 * CesiumMetadataPickingBlueprintLibrary.h.
 */

#include "Runtime/Public/Metadata/CesiumFeatureIdSet.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"

#if defined(CESIUM_GD_MODULE)
#include "core/math/vector3.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumPrimitiveFeaturesSnapshot;

class CesiumPrimitiveFeatures : public RefCounted {
	GDCLASS(CesiumPrimitiveFeatures, RefCounted)

public:
	void initialize(
		const std::shared_ptr<const CesiumPrimitiveFeaturesSnapshot>& snapshot
	);
	void add_feature_id_set(const Ref<CesiumFeatureIdSet>& featureIdSet);

	Array get_feature_id_sets() const;
	int32_t get_feature_id_set_count() const;
	Ref<CesiumFeatureIdSet> get_feature_id_set(int32_t index) const;
	int64_t get_vertex_count() const;
	int64_t get_instance_count() const;
	int64_t get_face_count() const;
	int64_t get_first_vertex_from_face(int64_t faceIndex) const;
	int64_t get_feature_id_from_face(
		int64_t faceIndex,
		int32_t featureIdSetIndex = 0
	) const;
	int64_t get_feature_id_from_hit(
		int64_t faceIndex,
		const Vector3& localPosition,
		int32_t featureIdSetIndex = 0
	) const;
	int64_t get_feature_id_from_instance(
		int64_t instanceIndex,
		int32_t featureIdSetIndex = 0
	) const;
	Dictionary get_metadata_values_for_face(
		const Ref<CesiumModelMetadata>& modelMetadata,
		int64_t faceIndex,
		int32_t featureIdSetIndex = 0
	) const;
	Dictionary get_metadata_values_from_hit(
		const Ref<CesiumModelMetadata>& modelMetadata,
		int64_t faceIndex,
		const Vector3& localPosition,
		int32_t featureIdSetIndex = 0
	) const;
	Dictionary get_metadata_values_for_instance(
		const Ref<CesiumModelMetadata>& modelMetadata,
		int64_t instanceIndex,
		int32_t featureIdSetIndex = 0
	) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumPrimitiveFeaturesSnapshot> m_snapshot;
	Array m_featureIdSets;
};

#endif // CESIUM_PRIMITIVE_FEATURES_H
