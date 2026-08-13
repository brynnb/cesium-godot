// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Georeference/CesiumGlobeAnchor.h"

#include "Godot/Nodes/CesiumGeoreference.h"
#include "Utils/CesiumMathUtils.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/core/error_macros.hpp"
#endif

#include <CesiumGeospatial/Cartographic.h>
#include <cmath>

namespace {
bool unpack_triple(const PackedFloat64Array& values, glm::dvec3* result) {
	if (values.size() != 3) {
		ERR_PRINT("An ECEF or longitude/latitude/height coordinate must contain three float64 values.");
		return false;
	}
	const glm::dvec3 candidate(values[0], values[1], values[2]);
	if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) ||
		!std::isfinite(candidate.z)) {
		ERR_PRINT("A globe-anchor coordinate must contain only finite values.");
		return false;
	}
	*result = candidate;
	return true;
}

PackedFloat64Array pack_triple(const glm::dvec3& value) {
	PackedFloat64Array result;
	result.resize(3);
	result.set(0, value.x);
	result.set(1, value.y);
	result.set(2, value.z);
	return result;
}
}

void CesiumGlobeAnchor::_enter_tree() {
	this->set_physics_process(this->m_automaticTransformSync);
	this->resolve_target();
	this->resolve_georeference();
	this->sync_from_target();
}

void CesiumGlobeAnchor::_exit_tree() {
	this->unregister_from_georeference();
	this->m_targetNode = ObjectID();
	this->m_lastTargetTransformValid = false;
}

void CesiumGlobeAnchor::_physics_process(double) {
	if (!this->m_automaticTransformSync || this->m_updatingTarget) {
		return;
	}
	Node3D* target = this->resolve_target();
	CesiumGeoreference* georeference = this->resolve_georeference();
	if (target == nullptr || georeference == nullptr) {
		return;
	}
	if (
		this->m_nativeAnchor.has_value() &&
		this->m_georeferenceRevision !=
			static_cast<uint64_t>(georeference->get_revision())
	) {
		this->sync_to_target();
		return;
	}
	const Transform3D current = target->get_global_transform();
	if (
		!this->m_lastTargetTransformValid ||
		!this->m_lastTargetGlobalTransform.is_equal_approx(current)
	) {
		this->sync_from_target();
	}
}

void CesiumGlobeAnchor::set_target_node_path(const NodePath& path) {
	if (this->m_targetNodePath == path) {
		return;
	}
	this->m_targetNodePath = path;
	this->m_targetNode = ObjectID();
	this->m_nativeAnchor.reset();
	this->m_lastTargetTransformValid = false;
	if (this->is_inside_tree()) {
		this->resolve_target();
		this->sync_from_target();
	}
}

NodePath CesiumGlobeAnchor::get_target_node_path() const {
	return this->m_targetNodePath;
}

void CesiumGlobeAnchor::set_georeference_path(const NodePath& path) {
	if (this->m_georeferencePath == path) {
		return;
	}
	this->unregister_from_georeference();
	this->m_georeferencePath = path;
	if (this->is_inside_tree()) {
		this->resolve_georeference();
		if (this->m_nativeAnchor.has_value()) {
			this->sync_to_target();
		} else {
			this->sync_from_target();
		}
	}
}

NodePath CesiumGlobeAnchor::get_georeference_path() const {
	return this->m_georeferencePath;
}

void CesiumGlobeAnchor::set_adjust_orientation_for_globe_when_moving(bool value) {
	this->m_adjustOrientationForGlobeWhenMoving = value;
}

bool CesiumGlobeAnchor::get_adjust_orientation_for_globe_when_moving() const {
	return this->m_adjustOrientationForGlobeWhenMoving;
}

void CesiumGlobeAnchor::set_automatic_transform_sync(bool value) {
	this->m_automaticTransformSync = value;
	this->set_physics_process(value);
}

bool CesiumGlobeAnchor::get_automatic_transform_sync() const {
	return this->m_automaticTransformSync;
}

Node3D* CesiumGlobeAnchor::resolve_target() {
	Node3D* cached = this->m_targetNode.is_null()
		? nullptr
		: Object::cast_to<Node3D>(ObjectDB::get_instance(this->m_targetNode));
	if (cached != nullptr) {
		return cached;
	}
	Node* candidate = this->m_targetNodePath.is_empty()
		? this->get_parent()
		: this->get_node_or_null(this->m_targetNodePath);
	Node3D* target = Object::cast_to<Node3D>(candidate);
	if (target != nullptr) {
		this->m_targetNode = ObjectID(target->get_instance_id());
	}
	return target;
}

Node3D* CesiumGlobeAnchor::get_target_node() {
	return this->resolve_target();
}

void CesiumGlobeAnchor::unregister_from_georeference() {
	if (this->m_georeference.is_null()) {
		return;
	}
	CesiumGeoreference* georeference = Object::cast_to<CesiumGeoreference>(
		ObjectDB::get_instance(this->m_georeference)
	);
	if (georeference != nullptr) {
		georeference->unregister_globe_anchor(this);
	}
	this->m_georeference = ObjectID();
}

CesiumGeoreference* CesiumGlobeAnchor::resolve_georeference() {
	CesiumGeoreference* cached = this->m_georeference.is_null()
		? nullptr
		: Object::cast_to<CesiumGeoreference>(
			ObjectDB::get_instance(this->m_georeference)
		);
	if (cached != nullptr) {
		return cached;
	}
	CesiumGeoreference* result = nullptr;
	if (!this->m_georeferencePath.is_empty()) {
		result = Object::cast_to<CesiumGeoreference>(
			this->get_node_or_null(this->m_georeferencePath)
		);
	} else {
		Node* current = this->resolve_target();
		while (current != nullptr && result == nullptr) {
			result = Object::cast_to<CesiumGeoreference>(current);
			current = current->get_parent();
		}
	}
	if (result != nullptr) {
		this->m_georeference = ObjectID(result->get_instance_id());
		this->m_georeferenceRevision = static_cast<uint64_t>(result->get_revision());
		result->register_globe_anchor(this);
	}
	return result;
}

CesiumGeoreference* CesiumGlobeAnchor::get_georeference() {
	return this->resolve_georeference();
}

bool CesiumGlobeAnchor::is_anchor_initialized() const {
	return this->m_nativeAnchor.has_value();
}

bool CesiumGlobeAnchor::target_transform_changed_since_last_sync() {
	Node3D* target = this->resolve_target();
	return target != nullptr &&
		(!this->m_lastTargetTransformValid ||
			!this->m_lastTargetGlobalTransform.is_equal_approx(
				target->get_global_transform()
			));
}

Transform3D CesiumGlobeAnchor::target_transform_in_georeference(
	Node3D* target,
	CesiumGeoreference* georeference
) const {
	return georeference->get_global_transform().affine_inverse() *
		target->get_global_transform();
}

bool CesiumGlobeAnchor::sync_from_target() {
	Node3D* target = this->resolve_target();
	CesiumGeoreference* georeference = this->resolve_georeference();
	if (target == nullptr || georeference == nullptr || this->m_updatingTarget) {
		return false;
	}
	const glm::dmat4 anchorToLocal = CesiumMathUtils::to_glm_mat4(
		this->target_transform_in_georeference(target, georeference)
	);
	if (!this->m_nativeAnchor.has_value()) {
		this->m_nativeAnchor =
			CesiumGeospatial::GlobeAnchor::fromAnchorToLocalTransform(
				georeference->get_coordinate_system(),
				anchorToLocal
			);
	} else {
		this->m_nativeAnchor->setAnchorToLocalTransform(
			georeference->get_coordinate_system(),
			anchorToLocal,
			this->m_adjustOrientationForGlobeWhenMoving,
			georeference->get_native_ellipsoid()
		);
	}
	++this->m_anchorRevision;
	this->m_georeferenceRevision =
		static_cast<uint64_t>(georeference->get_revision());
	this->sync_to_target();
	this->emit_signal("anchor_changed", static_cast<int64_t>(this->m_anchorRevision));
	return true;
}

bool CesiumGlobeAnchor::sync_to_target() {
	Node3D* target = this->resolve_target();
	CesiumGeoreference* georeference = this->resolve_georeference();
	if (
		target == nullptr || georeference == nullptr ||
		!this->m_nativeAnchor.has_value()
	) {
		return false;
	}
	const glm::dmat4 anchorToLocal =
		this->m_nativeAnchor->getAnchorToLocalTransform(
			georeference->get_coordinate_system()
		);
	const Transform3D local = CesiumMathUtils::from_glm_mat4(anchorToLocal);
	this->m_updatingTarget = true;
	target->set_global_transform(georeference->get_global_transform() * local);
	this->m_updatingTarget = false;
	this->m_lastTargetGlobalTransform = target->get_global_transform();
	this->m_lastTargetTransformValid = true;
	this->m_georeferenceRevision =
		static_cast<uint64_t>(georeference->get_revision());
	return true;
}

void CesiumGlobeAnchor::handle_georeference_changed() {
	if (this->m_nativeAnchor.has_value()) {
		this->sync_to_target();
	}
}

PackedFloat64Array CesiumGlobeAnchor::get_ecef_position_precise() const {
	if (!this->m_nativeAnchor.has_value()) {
		return PackedFloat64Array();
	}
	return pack_triple(glm::dvec3(
		this->m_nativeAnchor->getAnchorToFixedTransform()[3]
	));
}

bool CesiumGlobeAnchor::set_ecef_position_precise(
	const PackedFloat64Array& value
) {
	glm::dvec3 ecef;
	if (!unpack_triple(value, &ecef)) {
		return false;
	}
	CesiumGeoreference* georeference = this->resolve_georeference();
	glm::dmat4 transform = this->m_nativeAnchor.has_value()
		? this->m_nativeAnchor->getAnchorToFixedTransform()
		: glm::dmat4(1.0);
	transform[3] = glm::dvec4(ecef, 1.0);
	if (this->m_nativeAnchor.has_value()) {
		this->m_nativeAnchor->setAnchorToFixedTransform(
			transform,
			this->m_adjustOrientationForGlobeWhenMoving,
			georeference != nullptr
				? georeference->get_native_ellipsoid()
				: CesiumGeospatial::Ellipsoid::WGS84
		);
	} else {
		this->m_nativeAnchor =
			CesiumGeospatial::GlobeAnchor::fromAnchorToFixedTransform(transform);
	}
	++this->m_anchorRevision;
	this->sync_to_target();
	this->emit_signal("anchor_changed", static_cast<int64_t>(this->m_anchorRevision));
	return true;
}

PackedFloat64Array
CesiumGlobeAnchor::get_longitude_latitude_height_precise() const {
	if (!this->m_nativeAnchor.has_value()) {
		return PackedFloat64Array();
	}
	const CesiumGeoreference* georeference = this->m_georeference.is_null()
		? nullptr
		: Object::cast_to<CesiumGeoreference>(
			ObjectDB::get_instance(this->m_georeference)
		);
	const CesiumGeospatial::Ellipsoid& ellipsoid = georeference != nullptr
		? georeference->get_native_ellipsoid()
		: CesiumGeospatial::Ellipsoid::WGS84;
	const std::optional<CesiumGeospatial::Cartographic> cartographic =
		ellipsoid.cartesianToCartographic(glm::dvec3(
			this->m_nativeAnchor->getAnchorToFixedTransform()[3]
		));
	return cartographic.has_value()
		? pack_triple(glm::dvec3(
			Math::rad_to_deg(cartographic->longitude),
			Math::rad_to_deg(cartographic->latitude),
			cartographic->height
		))
		: PackedFloat64Array();
}

bool CesiumGlobeAnchor::set_longitude_latitude_height_precise(
	const PackedFloat64Array& value
) {
	glm::dvec3 llh;
	if (!unpack_triple(value, &llh)) {
		return false;
	}
	CesiumGeoreference* georeference = this->resolve_georeference();
	const CesiumGeospatial::Ellipsoid& ellipsoid = georeference != nullptr
		? georeference->get_native_ellipsoid()
		: CesiumGeospatial::Ellipsoid::WGS84;
	return this->set_ecef_position_precise(pack_triple(
		ellipsoid.cartographicToCartesian(CesiumGeospatial::Cartographic(
			Math::deg_to_rad(llh.x),
			Math::deg_to_rad(llh.y),
			llh.z
		))
	));
}

PackedFloat64Array CesiumGlobeAnchor::get_anchor_to_ecef_matrix_precise() const {
	PackedFloat64Array result;
	if (!this->m_nativeAnchor.has_value()) {
		return result;
	}
	result.resize(16);
	const glm::dmat4& matrix =
		this->m_nativeAnchor->getAnchorToFixedTransform();
	for (int32_t column = 0; column < 4; ++column) {
		for (int32_t row = 0; row < 4; ++row) {
			result.set(column * 4 + row, matrix[column][row]);
		}
	}
	return result;
}

bool CesiumGlobeAnchor::set_anchor_to_ecef_matrix_precise(
	const PackedFloat64Array& value
) {
	if (value.size() != 16) {
		ERR_PRINT("An anchor-to-ECEF matrix must contain 16 float64 values in column-major order.");
		return false;
	}
	glm::dmat4 matrix(1.0);
	for (int32_t column = 0; column < 4; ++column) {
		for (int32_t row = 0; row < 4; ++row) {
			const double component = value[column * 4 + row];
			if (!std::isfinite(component)) {
				ERR_PRINT("An anchor-to-ECEF matrix must contain only finite values.");
				return false;
			}
			matrix[column][row] = component;
		}
	}
	this->m_nativeAnchor =
		CesiumGeospatial::GlobeAnchor::fromAnchorToFixedTransform(matrix);
	++this->m_anchorRevision;
	this->sync_to_target();
	this->emit_signal("anchor_changed", static_cast<int64_t>(this->m_anchorRevision));
	return true;
}

int64_t CesiumGlobeAnchor::get_anchor_revision() const {
	return static_cast<int64_t>(this->m_anchorRevision);
}

void CesiumGlobeAnchor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node_path", "path"), &CesiumGlobeAnchor::set_target_node_path);
	ClassDB::bind_method(D_METHOD("get_target_node_path"), &CesiumGlobeAnchor::get_target_node_path);
	ClassDB::bind_method(D_METHOD("set_georeference_path", "path"), &CesiumGlobeAnchor::set_georeference_path);
	ClassDB::bind_method(D_METHOD("get_georeference_path"), &CesiumGlobeAnchor::get_georeference_path);
	ClassDB::bind_method(D_METHOD("set_adjust_orientation_for_globe_when_moving", "value"), &CesiumGlobeAnchor::set_adjust_orientation_for_globe_when_moving);
	ClassDB::bind_method(D_METHOD("get_adjust_orientation_for_globe_when_moving"), &CesiumGlobeAnchor::get_adjust_orientation_for_globe_when_moving);
	ClassDB::bind_method(D_METHOD("set_automatic_transform_sync", "value"), &CesiumGlobeAnchor::set_automatic_transform_sync);
	ClassDB::bind_method(D_METHOD("get_automatic_transform_sync"), &CesiumGlobeAnchor::get_automatic_transform_sync);
	ClassDB::bind_method(D_METHOD("get_target_node"), &CesiumGlobeAnchor::get_target_node);
	ClassDB::bind_method(D_METHOD("get_georeference"), &CesiumGlobeAnchor::get_georeference);
	ClassDB::bind_method(D_METHOD("is_anchor_initialized"), &CesiumGlobeAnchor::is_anchor_initialized);
	ClassDB::bind_method(D_METHOD("target_transform_changed_since_last_sync"), &CesiumGlobeAnchor::target_transform_changed_since_last_sync);
	ClassDB::bind_method(D_METHOD("sync_from_target"), &CesiumGlobeAnchor::sync_from_target);
	ClassDB::bind_method(D_METHOD("sync_to_target"), &CesiumGlobeAnchor::sync_to_target);
	ClassDB::bind_method(D_METHOD("get_ecef_position_precise"), &CesiumGlobeAnchor::get_ecef_position_precise);
	ClassDB::bind_method(D_METHOD("set_ecef_position_precise", "ecef"), &CesiumGlobeAnchor::set_ecef_position_precise);
	ClassDB::bind_method(D_METHOD("get_longitude_latitude_height_precise"), &CesiumGlobeAnchor::get_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("set_longitude_latitude_height_precise", "longitude_latitude_height"), &CesiumGlobeAnchor::set_longitude_latitude_height_precise);
	ClassDB::bind_method(D_METHOD("get_anchor_to_ecef_matrix_precise"), &CesiumGlobeAnchor::get_anchor_to_ecef_matrix_precise);
	ClassDB::bind_method(D_METHOD("set_anchor_to_ecef_matrix_precise", "matrix"), &CesiumGlobeAnchor::set_anchor_to_ecef_matrix_precise);
	ClassDB::bind_method(D_METHOD("get_anchor_revision"), &CesiumGlobeAnchor::get_anchor_revision);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_node_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_target_node_path", "get_target_node_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "georeference_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "CesiumGeoreference"), "set_georeference_path", "get_georeference_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "adjust_orientation_for_globe_when_moving"), "set_adjust_orientation_for_globe_when_moving", "get_adjust_orientation_for_globe_when_moving");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "automatic_transform_sync"), "set_automatic_transform_sync", "get_automatic_transform_sync");
	ADD_SIGNAL(MethodInfo("anchor_changed", PropertyInfo(Variant::INT, "revision")));
}
