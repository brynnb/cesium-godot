// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Godot adaptation of Cesium for Unreal's globe-anchored tile components.
// Last upstream review: Cesium for Unreal v2.29.0

#ifndef CESIUM_GEOREFERENCED_MESH_H
#define CESIUM_GEOREFERENCED_MESH_H

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#include "scene/3d/mesh_instance_3d.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/basis.hpp"
#include "godot_cpp/variant/vector3.hpp"
using namespace godot;
#endif

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Cesium3DTileset;
class CesiumGeoreference;

/** Internal tile mesh retaining an authoritative double-precision ECEF frame. */
class GeoreferencedMesh : public MeshInstance3D {
	GDCLASS(GeoreferencedMesh, MeshInstance3D)

public:
	void _ready() override;

	void set_anchor_to_ecef_transform(const glm::dmat4& transform);
	const glm::dmat4& get_anchor_to_ecef_transform() const;
	bool has_anchor_to_ecef_transform() const;
	void apply_georeference();
	void apply_position_on_globe(const glm::dvec3& ignoredCompatibilityOrigin);

	const glm::dvec3& get_original_position() const;
	void set_original_position(const glm::dvec3& position);
	const Basis& get_original_basis() const;
	void set_original_basis(const Basis& basis);

	void set_engine_position(const Vector3& position);
	void set_ecef_position(const Vector3& position);
	Vector3 get_ecef_position() const;
	Vector3 get_engine_position() const;

	void set_tileset(Cesium3DTileset* tileset);
	void set_tileset_no_reparent(Cesium3DTileset* tileset);
	Cesium3DTileset* get_tileset() const;
	CesiumGeoreference* get_georeference() const;

protected:
	static void _bind_methods();

private:
	glm::dmat4 m_anchorToEcef{1.0};
	bool m_anchorToEcefValid = false;
	glm::dvec3 m_originalPosition{0.0};
	Basis m_originalBasis;
	ObjectID m_tileset;
};

#endif
