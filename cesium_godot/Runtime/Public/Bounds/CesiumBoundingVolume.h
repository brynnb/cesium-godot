// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#ifndef CESIUM_BOUNDING_VOLUME_H
#define CESIUM_BOUNDING_VOLUME_H

/**
 * Godot adaptation reviewed against Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/CalcBounds.* and Public/CesiumTile.h.
 *
 * This Resource exposes an owned Cesium Native bounding volume without
 * leaking renderer or tile pointers through the Godot API.
 */

#if defined(CESIUM_GD_MODULE)
#include "core/math/aabb.h"
#include "core/math/transform_3d.h"
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/aabb.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/transform3d.hpp"
using namespace godot;
#endif

#include <memory>

struct CesiumBoundingVolumeSnapshot;

class CesiumBoundingVolume : public RefCounted {
	GDCLASS(CesiumBoundingVolume, RefCounted)

public:
	enum Type {
		Invalid = 0,
		Sphere = 1,
		OrientedBox = 2,
		Region = 3,
		RegionWithLooseFittingHeights = 4,
		S2Cell = 5,
		CylinderRegion = 6,
	};

	void initialize(
		const std::shared_ptr<const CesiumBoundingVolumeSnapshot>& snapshot
	);
	bool is_valid() const;
	int32_t get_volume_type() const;
	String get_volume_type_name() const;
	Vector3 get_center() const;
	PackedFloat64Array get_center_components() const;
	Transform3D get_oriented_box_transform() const;
	PackedFloat64Array get_oriented_box_half_axes() const;
	Vector3 get_oriented_box_size() const;
	AABB get_source_aabb() const;
	AABB get_transformed_aabb(const Transform3D& sourceToTarget) const;
	Dictionary get_details() const;
	Ref<CesiumBoundingVolume> create_owned_copy() const;
	bool contains_source_position(const Vector3& position) const;
	bool contains_source_position_components(
		const PackedFloat64Array& position
	) const;
	double distance_to_source_position(const Vector3& position) const;
	double distance_to_source_position_components(
		const PackedFloat64Array& position
	) const;

protected:
	static void _bind_methods();

private:
	std::shared_ptr<const CesiumBoundingVolumeSnapshot> m_snapshot;
};

VARIANT_ENUM_CAST(CesiumBoundingVolume::Type);

#endif // CESIUM_BOUNDING_VOLUME_H
