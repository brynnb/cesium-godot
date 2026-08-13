// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumOriginShift.h"

#include "Godot/Nodes/CesiumGeoreference.h"
#include "Runtime/Public/Georeference/CesiumGlobeAnchor.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/core/error_macros.hpp"
#endif

#include <algorithm>

void CesiumOriginShift::_enter_tree() {
	this->set_physics_process(this->m_mode != Disabled);
	this->resolve_globe_anchor();
}

void CesiumOriginShift::_physics_process(double) {
	if (this->m_mode == Disabled) {
		return;
	}
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	if (anchor == nullptr) {
		if (!this->m_warnedMissingAnchor) {
			WARN_PRINT("CesiumOriginShift requires a CesiumGlobeAnchor on its target.");
			this->m_warnedMissingAnchor = true;
		}
		return;
	}
	this->m_warnedMissingAnchor = false;
	Node3D* target = anchor->get_target_node();
	CesiumGeoreference* georeference = anchor->get_georeference();
	if (
		target == nullptr || georeference == nullptr ||
		georeference->get_origin_type_raw() !=
			CesiumGeoreference::CartographicOrigin
	) {
		return;
	}
	const Vector3 local = georeference->get_global_transform()
		.affine_inverse()
		.xform(target->get_global_position());
	const double distanceSquared = static_cast<double>(local.length_squared());
	if (
		(this->m_distance > 0.0 &&
			distanceSquared <= this->m_distance * this->m_distance) ||
		(this->m_distance == 0.0 && distanceSquared <= 1.0e-12)
	) {
		return;
	}
	this->shift_origin_now();
}

void CesiumOriginShift::set_mode(int32_t mode) {
	this->m_mode = mode == ChangeCesiumGeoreference
		? ChangeCesiumGeoreference
		: Disabled;
	this->set_physics_process(this->m_mode != Disabled);
}

int32_t CesiumOriginShift::get_mode() const {
	return static_cast<int32_t>(this->m_mode);
}

void CesiumOriginShift::set_distance(double distance) {
	this->m_distance = std::max(0.0, distance);
}

double CesiumOriginShift::get_distance() const {
	return this->m_distance;
}

void CesiumOriginShift::set_globe_anchor_path(const NodePath& path) {
	this->m_globeAnchorPath = path;
	this->m_globeAnchor = ObjectID();
	this->resolve_globe_anchor();
}

NodePath CesiumOriginShift::get_globe_anchor_path() const {
	return this->m_globeAnchorPath;
}

CesiumGlobeAnchor* CesiumOriginShift::resolve_globe_anchor() {
	CesiumGlobeAnchor* cached = this->m_globeAnchor.is_null()
		? nullptr
		: Object::cast_to<CesiumGlobeAnchor>(
			ObjectDB::get_instance(this->m_globeAnchor)
		);
	if (cached != nullptr) {
		return cached;
	}
	CesiumGlobeAnchor* result = nullptr;
	if (!this->m_globeAnchorPath.is_empty()) {
		result = Object::cast_to<CesiumGlobeAnchor>(
			this->get_node_or_null(this->m_globeAnchorPath)
		);
	} else {
		Node* parent = this->get_parent();
		if (parent != nullptr) {
			for (int32_t i = 0; i < parent->get_child_count(); ++i) {
				result = Object::cast_to<CesiumGlobeAnchor>(parent->get_child(i));
				if (result != nullptr) {
					break;
				}
			}
		}
	}
	if (result != nullptr) {
		this->m_globeAnchor = ObjectID(result->get_instance_id());
	}
	return result;
}

CesiumGlobeAnchor* CesiumOriginShift::get_globe_anchor() {
	return this->resolve_globe_anchor();
}

bool CesiumOriginShift::shift_origin_now() {
	CesiumGlobeAnchor* anchor = this->resolve_globe_anchor();
	if (anchor == nullptr || !anchor->sync_from_target()) {
		return false;
	}
	CesiumGeoreference* georeference = anchor->get_georeference();
	if (
		georeference == nullptr ||
		georeference->get_origin_type_raw() !=
			CesiumGeoreference::CartographicOrigin
	) {
		return false;
	}
	const PackedFloat64Array oldOrigin = georeference->get_origin_ecef_precise();
	const PackedFloat64Array newOrigin = anchor->get_ecef_position_precise();
	if (newOrigin.size() != 3 || !georeference->set_origin_ecef_precise(newOrigin)) {
		return false;
	}
	++this->m_shiftCount;
	this->emit_signal(
		"origin_shifted",
		oldOrigin,
		newOrigin,
		static_cast<int64_t>(this->m_shiftCount)
	);
	return true;
}

int64_t CesiumOriginShift::get_shift_count() const {
	return static_cast<int64_t>(this->m_shiftCount);
}

void CesiumOriginShift::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &CesiumOriginShift::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &CesiumOriginShift::get_mode);
	ClassDB::bind_method(D_METHOD("set_distance", "distance"), &CesiumOriginShift::set_distance);
	ClassDB::bind_method(D_METHOD("get_distance"), &CesiumOriginShift::get_distance);
	ClassDB::bind_method(D_METHOD("set_globe_anchor_path", "path"), &CesiumOriginShift::set_globe_anchor_path);
	ClassDB::bind_method(D_METHOD("get_globe_anchor_path"), &CesiumOriginShift::get_globe_anchor_path);
	ClassDB::bind_method(D_METHOD("get_globe_anchor"), &CesiumOriginShift::get_globe_anchor);
	ClassDB::bind_method(D_METHOD("shift_origin_now"), &CesiumOriginShift::shift_origin_now);
	ClassDB::bind_method(D_METHOD("get_shift_count"), &CesiumOriginShift::get_shift_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Disabled,Change Cesium Georeference"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance", PROPERTY_HINT_RANGE, "0,1000000000,1,or_greater,exp,suffix:m"), "set_distance", "get_distance");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "globe_anchor_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "CesiumGlobeAnchor"), "set_globe_anchor_path", "get_globe_anchor_path");
	BIND_ENUM_CONSTANT(Disabled);
	BIND_ENUM_CONSTANT(ChangeCesiumGeoreference);
	ADD_SIGNAL(MethodInfo(
		"origin_shifted",
		PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "old_origin_ecef"),
		PropertyInfo(Variant::PACKED_FLOAT64_ARRAY, "new_origin_ecef"),
		PropertyInfo(Variant::INT, "shift_count")
	));
}
