#ifndef CESIUM_LOADED_TILE_PRIMITIVE_H
#define CESIUM_LOADED_TILE_PRIMITIVE_H

#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveMetadata.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/variant/array.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/geometry_instance3d.hpp"
#include "godot_cpp/variant/basis.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include "godot_cpp/variant/variant.hpp"
using namespace godot;
#endif

class Cesium3DTile;
class CesiumGltfInstancedComponent;

/**
 * Godot adaptation of Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Public/CesiumLoadedTile.h
 *
 * A stable, script-accessible description of one glTF mesh primitive realized
 * as a surface on a loaded Cesium tile.
 *
 * The primitive copies its informational glTF data. It deliberately stores the
 * tile as an ObjectID so retaining this RefCounted after tile unload cannot
 * leave a dangling scene-node pointer.
 */
class CesiumLoadedTilePrimitive : public RefCounted {
	GDCLASS(CesiumLoadedTilePrimitive, RefCounted)

public:
	Cesium3DTile* get_loaded_tile() const;
	GeometryInstance3D* get_render_node() const;
	String get_tile_id() const;
	Variant get_tile_extras() const;
	int32_t get_surface_index() const;
	int32_t get_mesh_surface_index() const;
	bool is_gpu_instanced() const;
	int32_t get_instance_count() const;
	int32_t get_node_index() const;
	int32_t get_mesh_index() const;
	int32_t get_primitive_index() const;
	int32_t get_material_index() const;
	int32_t get_primitive_mode() const;
	String get_primitive_mode_name() const;
	bool is_point_primitive() const;
	bool is_line_primitive() const;
	bool is_triangle_primitive() const;
	bool is_translucent() const;
	bool is_translucency_isolated() const;
	int64_t get_point_count() const;
	int64_t get_point_diameter() const;
	Vector3 get_primitive_dimensions() const;
	double get_tile_geometric_error() const;
	bool get_uses_additive_refinement() const;
	Dictionary get_point_cloud_parameters() const;
	Dictionary get_attributes() const;
	Dictionary get_gltf_material() const;
	Dictionary get_texture_coordinate_mappings() const;
	Ref<Material> get_default_material() const;
	Ref<Material> get_selected_base_material() const;
	Ref<Material> get_active_material() const;
	bool get_uses_custom_material() const;
	Array get_raster_overlay_bindings() const;
	Dictionary get_render_diagnostics() const;
	Ref<CesiumPrimitiveFeatures> get_primitive_features() const;
	Ref<CesiumPrimitiveFeatures> get_instance_features() const;
	Ref<CesiumPrimitiveMetadata> get_primitive_metadata() const;
	Ref<CesiumModelMetadata> get_model_metadata() const;
	int32_t get_texture_coordinate_index(const String& semantic) const;
	int32_t get_overlay_texture_coordinate_index(int32_t textureCoordinateId) const;
	Vector3 get_absolute_origin() const;
	Vector3 get_absolute_origin_high() const;
	Vector3 get_absolute_origin_low() const;
	Basis get_local_to_absolute_basis() const;
	bool apply_world_coordinate_parameters(
		const Ref<Material>& material,
		const String& parameterPrefix = "cesium"
	) const;

	// C++ renderer integration only. All values exposed to scripts are copied.
	void initialize(
		Cesium3DTile* tile,
		GeometryInstance3D* renderNode,
		const String& tileId,
		int32_t rendererPrimitiveIndex,
		int32_t meshSurfaceIndex,
		int32_t nodeIndex,
		int32_t meshIndex,
		int32_t primitiveIndex,
		int32_t materialIndex,
		int32_t primitiveMode,
		int64_t pointCount,
		int64_t pointDiameter,
		const Vector3& primitiveDimensions,
		double tileGeometricError,
		bool usesAdditiveRefinement,
		const Dictionary& attributes,
		const Dictionary& gltfMaterial,
		const Dictionary& textureCoordinateMappings,
		const Ref<Material>& defaultMaterial,
		const Ref<CesiumPrimitiveFeatures>& primitiveFeatures,
		const Ref<CesiumPrimitiveFeatures>& instanceFeatures,
		const Ref<CesiumPrimitiveMetadata>& primitiveMetadata,
		const Ref<CesiumModelMetadata>& modelMetadata
	);
	void set_selected_base_material(
		const Ref<Material>& material,
		bool customMaterial
	);

protected:
	static void _bind_methods();

private:
	ObjectID m_tile;
	ObjectID m_renderNode;
	String m_tileId;
	Variant m_tileExtras;
	int32_t m_surfaceIndex = -1;
	int32_t m_meshSurfaceIndex = -1;
	bool m_gpuInstanced = false;
	int32_t m_instanceCount = 0;
	int32_t m_nodeIndex = -1;
	int32_t m_meshIndex = -1;
	int32_t m_primitiveIndex = -1;
	int32_t m_materialIndex = -1;
	int32_t m_primitiveMode = -1;
	int64_t m_pointCount = 0;
	int64_t m_pointDiameter = 0;
	Vector3 m_primitiveDimensions;
	double m_tileGeometricError = 0.0;
	bool m_usesAdditiveRefinement = false;
	bool m_translucent = false;
	bool m_translucencyIsolated = false;
	Dictionary m_attributes;
	Dictionary m_gltfMaterial;
	Dictionary m_textureCoordinateMappings;
	ObjectID m_defaultMaterial;
	ObjectID m_selectedBaseMaterial;
	bool m_usesCustomMaterial = false;
	Ref<CesiumPrimitiveFeatures> m_primitiveFeatures;
	Ref<CesiumPrimitiveFeatures> m_instanceFeatures;
	Ref<CesiumPrimitiveMetadata> m_primitiveMetadata;
	Ref<CesiumModelMetadata> m_modelMetadata;
	Vector3 m_absoluteOrigin;
	Vector3 m_absoluteOriginHigh;
	Vector3 m_absoluteOriginLow;
	Basis m_localToAbsoluteBasis;
};

#endif // CESIUM_LOADED_TILE_PRIMITIVE_H
