// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Godot/Nodes/CesiumGeoreferencedMesh.h"

#include "Godot/Nodes/CesiumGDTileset.h"
#include "Godot/Nodes/CesiumGeoreference.h"
#include "Utils/CesiumMathUtils.h"

void GeoreferencedMesh::_ready() {
	this->apply_georeference();
}

void GeoreferencedMesh::set_anchor_to_ecef_transform(
	const glm::dmat4& transform
) {
	this->m_anchorToEcef = transform;
	this->m_anchorToEcefValid = true;
	this->apply_georeference();
}

const glm::dmat4& GeoreferencedMesh::get_anchor_to_ecef_transform() const {
	return this->m_anchorToEcef;
}

bool GeoreferencedMesh::has_anchor_to_ecef_transform() const {
	return this->m_anchorToEcefValid;
}

void GeoreferencedMesh::apply_georeference() {
	CesiumGeoreference* georeference = this->get_georeference();
	if (georeference == nullptr || !this->m_anchorToEcefValid) {
		return;
	}
	const Transform3D local = CesiumMathUtils::from_glm_mat4(
		georeference->ecef_transform_to_local(this->m_anchorToEcef)
	);
	this->set_global_transform(georeference->get_global_transform() * local);
}

void GeoreferencedMesh::apply_position_on_globe(const glm::dvec3&) {
	this->apply_georeference();
}

const glm::dvec3& GeoreferencedMesh::get_original_position() const {
	return this->m_originalPosition;
}

void GeoreferencedMesh::set_original_position(const glm::dvec3& position) {
	this->m_originalPosition = position;
}

const Basis& GeoreferencedMesh::get_original_basis() const {
	return this->m_originalBasis;
}

void GeoreferencedMesh::set_original_basis(const Basis& basis) {
	this->m_originalBasis = basis;
}

void GeoreferencedMesh::set_engine_position(const Vector3& position) {
	CesiumGeoreference* georeference = this->get_georeference();
	if (georeference == nullptr) {
		return;
	}
	glm::dmat4 local(1.0);
	local[3] = glm::dvec4(
		static_cast<double>(position.x),
		static_cast<double>(position.y),
		static_cast<double>(position.z),
		1.0
	);
	this->set_anchor_to_ecef_transform(
		georeference->local_transform_to_ecef(local)
	);
}

void GeoreferencedMesh::set_ecef_position(const Vector3& position) {
	this->m_anchorToEcef[3] = glm::dvec4(
		static_cast<double>(position.x),
		static_cast<double>(position.y),
		static_cast<double>(position.z),
		1.0
	);
	this->m_anchorToEcefValid = true;
	this->apply_georeference();
}

Vector3 GeoreferencedMesh::get_ecef_position() const {
	return this->m_anchorToEcefValid
		? CesiumMathUtils::from_glm_vec3(glm::dvec3(this->m_anchorToEcef[3]))
		: Vector3();
}

Vector3 GeoreferencedMesh::get_engine_position() const {
	CesiumGeoreference* georeference = this->get_georeference();
	return georeference != nullptr && this->m_anchorToEcefValid
		? CesiumMathUtils::from_glm_vec3(
			georeference->get_coordinate_system().ecefPositionToLocal(
				glm::dvec3(this->m_anchorToEcef[3])
			)
		)
		: Vector3();
}

void GeoreferencedMesh::set_tileset(Cesium3DTileset* tileset) {
	this->m_tileset = tileset == nullptr
		? ObjectID()
		: ObjectID(tileset->get_instance_id());
	this->apply_georeference();
}

void GeoreferencedMesh::set_tileset_no_reparent(Cesium3DTileset* tileset) {
	this->set_tileset(tileset);
}

Cesium3DTileset* GeoreferencedMesh::get_tileset() const {
	return this->m_tileset.is_null()
		? nullptr
		: Object::cast_to<Cesium3DTileset>(
			ObjectDB::get_instance(this->m_tileset)
		);
}

CesiumGeoreference* GeoreferencedMesh::get_georeference() const {
	Cesium3DTileset* tileset = this->get_tileset();
	return tileset == nullptr ? nullptr : tileset->get_georeference_node();
}

void GeoreferencedMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_engine_position"), &GeoreferencedMesh::get_engine_position);
	ClassDB::bind_method(D_METHOD("set_engine_position", "position"), &GeoreferencedMesh::set_engine_position);
	ClassDB::bind_method(D_METHOD("get_ecef_position"), &GeoreferencedMesh::get_ecef_position);
	ClassDB::bind_method(D_METHOD("set_ecef_position", "position"), &GeoreferencedMesh::set_ecef_position);
	ClassDB::bind_method(D_METHOD("set_tileset", "tileset"), &GeoreferencedMesh::set_tileset);
	ClassDB::bind_method(D_METHOD("get_tileset"), &GeoreferencedMesh::get_tileset);
	ClassDB::bind_method(D_METHOD("get_georeference"), &GeoreferencedMesh::get_georeference);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tileset", PROPERTY_HINT_NODE_TYPE, "Cesium3DTileset"), "set_tileset", "get_tileset");
}
