#ifndef CESIUM_3D_TILE
#define CESIUM_3D_TILE

#include "Godot/Nodes/CesiumGeoreferencedMesh.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/variant.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/geometry_instance3d.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/concave_polygon_shape3d.hpp"
using namespace godot;
#endif

struct CesiumGltfSharedImageResource;
struct CesiumGltfSharedModelResource;

struct CesiumTileCollisionPrimitive {
	int32_t rendererPrimitiveIndex = -1;
	Ref<ArrayMesh> mesh;
	int32_t meshSurfaceIndex = -1;
};

class Cesium3DTile : public GeoreferencedMesh {
	GDCLASS(Cesium3DTile, GeoreferencedMesh)
	
public:
	~Cesium3DTile() override;
	
	void _ready() override;
	
	void generate_tile_collision();
	void set_tile_collision_enabled(bool enabled);
	
	void set_model_metadata(const Ref<CesiumModelMetadata>& metadata);
	Ref<CesiumModelMetadata> get_model_metadata() const;
	void set_primitive_features(const Array& primitiveFeatures);
	Array get_all_primitive_features() const;
	Ref<CesiumPrimitiveFeatures> get_primitive_features(
		int32_t surfaceIndex
	) const;
	
	Dictionary get_metadata_table(int32_t idx) const;

	int32_t get_table_count() const;

	Dictionary get_structural_metadata() const;

	void set_tile_id(const String& tileId);

	const String& get_tile_id() const;
	void set_tile_extras(const Variant& tileExtras);
	Variant get_tile_extras() const;
	void set_bounding_volumes(
		const Ref<CesiumBoundingVolume>& tileBounds,
		const Ref<CesiumBoundingVolume>& contentBounds,
		const Ref<CesiumBoundingVolume>& viewerRequestBounds
	);
	Ref<CesiumBoundingVolume> get_tile_bounds() const;
	Ref<CesiumBoundingVolume> get_content_bounds() const;
	Ref<CesiumBoundingVolume> get_viewer_request_bounds() const;
	AABB get_tile_source_aabb() const;

	Array get_loaded_tile_primitives() const;
	int32_t get_loaded_tile_primitive_count() const;
	Ref<CesiumLoadedTilePrimitive> get_loaded_tile_primitive(int32_t surfaceIndex) const;
	Ref<CesiumLoadedTilePrimitive> get_loaded_tile_primitive_for_mesh_surface(
		int32_t meshSurfaceIndex
	) const;
	Ref<CesiumLoadedTilePrimitive> get_loaded_tile_primitive_for_collision_face(
		int64_t globalFaceIndex,
		int64_t* localFaceIndex = nullptr
	) const;
	Array get_all_raster_overlay_bindings() const;
	Array get_raster_overlay_bindings(int32_t surfaceIndex = -1) const;

	// Renderer integration only.
	void set_loaded_tile_primitives(
		const std::vector<Ref<CesiumLoadedTilePrimitive>>& primitives,
		const std::vector<Ref<Material>>& baseSurfaceMaterials
	);
	void set_primitive_render_nodes(
		const std::vector<GeometryInstance3D*>& renderNodes,
		const std::vector<int32_t>& meshSurfaceIndices,
		const std::vector<int32_t>& parentSurfaceToPrimitiveIndex
	);
	void generate_tile_collision_from_primitives(
		const std::vector<CesiumTileCollisionPrimitive>& primitives
	);
	void set_shared_image_resources(
		std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>&& resources
	);
	void set_shared_model_resource(
		const std::shared_ptr<CesiumGltfSharedModelResource>& resource
	);
	GeometryInstance3D* get_primitive_render_node(
		int32_t rendererPrimitiveIndex
	) const;
	int32_t get_primitive_mesh_surface_index(
		int32_t rendererPrimitiveIndex
	) const;
	void set_primitive_override_material(
		int32_t rendererPrimitiveIndex,
		const Ref<Material>& material
	);
	Ref<CesiumRasterOverlayBinding> add_raster_overlay_binding(
		const Ref<CesiumRasterOverlayBinding>& binding
	);
	Ref<CesiumRasterOverlayBinding> remove_raster_overlay_binding(
		int32_t surfaceIndex,
		const String& overlayKey,
		const Ref<Texture2D>& expectedTexture = Ref<Texture2D>()
	);
	Array take_raster_overlay_bindings();
	void refresh_raster_overlay_material(int32_t surfaceIndex);
	
private:

	Node* create_collision_node_custom_trimesh(
		const Ref<ConcavePolygonShape3D>& shape
	);

	Ref<CesiumModelMetadata> m_modelMetadata;
	Array m_primitiveFeatures;

	String m_tileId;
	Variant m_tileExtras;
	Ref<CesiumBoundingVolume> m_tileBounds;
	Ref<CesiumBoundingVolume> m_contentBounds;
	Ref<CesiumBoundingVolume> m_viewerRequestBounds;

	std::vector<Ref<CesiumLoadedTilePrimitive>> m_loadedTilePrimitives;
	std::vector<ObjectID> m_primitiveRenderNodes;
	std::vector<int32_t> m_primitiveMeshSurfaceIndices;
	std::vector<int32_t> m_parentSurfaceToPrimitiveIndex;
	std::vector<int32_t> m_collisionFacePrimitiveIndices;
	std::vector<int64_t> m_collisionFaceCounts;
	std::vector<Ref<Material>> m_baseSurfaceMaterials;
	std::vector<Ref<CesiumRasterOverlayBinding>> m_rasterOverlayBindings;
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>
		m_sharedImageResources;
	std::shared_ptr<CesiumGltfSharedModelResource> m_sharedModelResource;
	ObjectID m_collisionShape;

protected:

	static void _bind_methods();
	
};


#endif
