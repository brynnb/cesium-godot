// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.
// Upstream counterparts:
// - Source/CesiumRuntime/Public/CesiumOriginShiftComponent.h
// - Source/CesiumRuntime/Private/CesiumOriginShiftComponent.cpp
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_ORIGIN_SHIFT_H
#define CESIUM_ORIGIN_SHIFT_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "core/variant/node_path.h"
#include "scene/main/node.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/node_path.hpp"
using namespace godot;
#endif

class CesiumGlobeAnchor;

/** Keeps an anchored target near the Godot origin by moving its georeference. */
class CesiumOriginShift : public Node {
	GDCLASS(CesiumOriginShift, Node)

public:
	enum Mode {
		Disabled = 0,
		ChangeCesiumGeoreference = 1,
	};

	void _enter_tree() override;
	void _physics_process(double delta) override;

	void set_mode(int32_t mode);
	int32_t get_mode() const;
	void set_distance(double distance);
	double get_distance() const;
	void set_globe_anchor_path(const NodePath& path);
	NodePath get_globe_anchor_path() const;
	CesiumGlobeAnchor* get_globe_anchor();
	bool shift_origin_now();
	int64_t get_shift_count() const;

protected:
	static void _bind_methods();

private:
	CesiumGlobeAnchor* resolve_globe_anchor();

	Mode m_mode = Disabled;
	double m_distance = 10000.0;
	NodePath m_globeAnchorPath;
	ObjectID m_globeAnchor;
	uint64_t m_shiftCount = 0;
	bool m_warnedMissingAnchor = false;
};

VARIANT_ENUM_CAST(CesiumOriginShift::Mode);

#endif
