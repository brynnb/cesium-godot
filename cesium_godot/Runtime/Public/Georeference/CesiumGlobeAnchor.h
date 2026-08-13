// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumGlobeAnchorComponent.h
// - Source/CesiumRuntime/Private/CesiumGlobeAnchorComponent.cpp
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_GLOBE_ANCHOR_H
#define CESIUM_GLOBE_ANCHOR_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/variant/node_path.h"
#include "core/variant/packed_float64_array.h"
#include "scene/3d/node_3d.h"
#include "scene/main/node.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/node_path.hpp"
#include "godot_cpp/variant/packed_float64_array.hpp"
#include "godot_cpp/variant/transform3d.hpp"
using namespace godot;
#endif

#include <CesiumGeospatial/GlobeAnchor.h>
#include <optional>

class CesiumGeoreference;

/**
 * Godot component-style globe anchor for any ordinary Node3D.
 *
 * Add it as a child of the Node3D to anchor, or assign `target_node_path`.
 * The component stores the authoritative object-to-ECEF transform in float64
 * and re-realizes the small Godot transform whenever the georeference moves.
 */
class CesiumGlobeAnchor : public Node {
	GDCLASS(CesiumGlobeAnchor, Node)

public:
	void _enter_tree() override;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void set_target_node_path(const NodePath& path);
	NodePath get_target_node_path() const;
	void set_georeference_path(const NodePath& path);
	NodePath get_georeference_path() const;
	void set_adjust_orientation_for_globe_when_moving(bool value);
	bool get_adjust_orientation_for_globe_when_moving() const;
	void set_automatic_transform_sync(bool value);
	bool get_automatic_transform_sync() const;

	Node3D* get_target_node();
	CesiumGeoreference* get_georeference();
	bool is_anchor_initialized() const;
	bool target_transform_changed_since_last_sync();

	bool sync_from_target();
	bool sync_to_target();
	void handle_georeference_changed();

	PackedFloat64Array get_ecef_position_precise() const;
	bool set_ecef_position_precise(const PackedFloat64Array& value);
	PackedFloat64Array get_longitude_latitude_height_precise() const;
	bool set_longitude_latitude_height_precise(const PackedFloat64Array& value);
	PackedFloat64Array get_anchor_to_ecef_matrix_precise() const;
	bool set_anchor_to_ecef_matrix_precise(const PackedFloat64Array& value);

	int64_t get_anchor_revision() const;

protected:
	static void _bind_methods();

private:
	Node3D* resolve_target();
	CesiumGeoreference* resolve_georeference();
	void unregister_from_georeference();
	Transform3D target_transform_in_georeference(
		Node3D* target,
		CesiumGeoreference* georeference
	) const;

	NodePath m_targetNodePath;
	NodePath m_georeferencePath;
	ObjectID m_targetNode;
	ObjectID m_georeference;
	std::optional<CesiumGeospatial::GlobeAnchor> m_nativeAnchor;
	Transform3D m_lastTargetGlobalTransform;
	bool m_lastTargetTransformValid = false;
	bool m_adjustOrientationForGlobeWhenMoving = true;
	bool m_automaticTransformSync = true;
	bool m_updatingTarget = false;
	uint64_t m_georeferenceRevision = 0;
	uint64_t m_anchorRevision = 0;
};

#endif
