#include "Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Runtime/Private/Renderer/CesiumGltfModelResourceCache.h"
#include "godot_cpp/classes/collision_shape3d.hpp"
#include "godot_cpp/classes/concave_polygon_shape3d.hpp"
#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/classes/static_body3d.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <utility>
#include <vector>

Cesium3DTile::~Cesium3DTile() {
	// Godot's renderer stores material RIDs on GeometryInstance3D. Remove the
	// per-placement overrides while this derived class still owns the Material
	// Refs; otherwise the refs can be destroyed before MeshInstance3D performs
	// its base-class renderer teardown.
	for (int32_t index = 0;
		index < static_cast<int32_t>(this->m_primitiveRenderNodes.size());
		++index) {
		this->set_primitive_override_material(index, Ref<Material>());
	}
}


void Cesium3DTile::_ready() {
	// Override to delete behavior
}

void Cesium3DTile::generate_tile_collision() {
	std::vector<CesiumTileCollisionPrimitive> primitives;
	const Ref<ArrayMesh> mesh = this->get_mesh();
	if (mesh.is_valid()) {
		for (int32_t surfaceIndex = 0;
			surfaceIndex < mesh->get_surface_count();
			++surfaceIndex) {
			if (
				mesh->surface_get_primitive_type(surfaceIndex) !=
				Mesh::PRIMITIVE_TRIANGLES
			) {
				continue;
			}
			const int32_t rendererPrimitiveIndex =
				surfaceIndex >= 0 && surfaceIndex < static_cast<int32_t>(
					this->m_parentSurfaceToPrimitiveIndex.size()
				)
					? this->m_parentSurfaceToPrimitiveIndex[surfaceIndex]
					: surfaceIndex;
			primitives.push_back({
				rendererPrimitiveIndex,
				mesh,
				surfaceIndex
			});
		}
	}
	this->generate_tile_collision_from_primitives(primitives);
}

void Cesium3DTile::generate_tile_collision_from_primitives(
	const std::vector<CesiumTileCollisionPrimitive>& primitives
) {
	PackedVector3Array facePoints;
	this->m_collisionFacePrimitiveIndices.clear();
	this->m_collisionFaceCounts.clear();

	for (const CesiumTileCollisionPrimitive& primitive : primitives) {
		if (
			primitive.mesh.is_null() || primitive.meshSurfaceIndex < 0 ||
			primitive.meshSurfaceIndex >= primitive.mesh->get_surface_count() ||
			primitive.mesh->surface_get_primitive_type(
				primitive.meshSurfaceIndex
			) != Mesh::PRIMITIVE_TRIANGLES
		) {
			continue;
		}
		const Array arrays = primitive.mesh->surface_get_arrays(
			primitive.meshSurfaceIndex
		);
		if (arrays.size() <= Mesh::ARRAY_VERTEX) {
			continue;
		}
		const PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		const PackedInt32Array indices = arrays.size() > Mesh::ARRAY_INDEX
			? static_cast<PackedInt32Array>(arrays[Mesh::ARRAY_INDEX])
			: PackedInt32Array();
		const int64_t elementCount = indices.is_empty()
			? vertices.size()
			: indices.size();
		const int64_t faceCount = elementCount / 3;
		if (faceCount <= 0) {
			continue;
		}
		const int64_t before = facePoints.size();
		for (int64_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
			const int64_t element = faceIndex * 3;
			const int64_t i0 = indices.is_empty() ? element : indices[element];
			const int64_t i1 = indices.is_empty() ? element + 1 : indices[element + 1];
			const int64_t i2 = indices.is_empty() ? element + 2 : indices[element + 2];
			if (
				i0 < 0 || i1 < 0 || i2 < 0 ||
				i0 >= vertices.size() || i1 >= vertices.size() ||
				i2 >= vertices.size()
			) {
				continue;
			}
			// Preserve the renderer's established inverse-winding collision path.
			facePoints.push_back(vertices[i2]);
			facePoints.push_back(vertices[i1]);
			facePoints.push_back(vertices[i0]);
		}
		const int64_t realizedFaceCount =
			(facePoints.size() - before) / 3;
		if (realizedFaceCount > 0) {
			this->m_collisionFacePrimitiveIndices.push_back(
				primitive.rendererPrimitiveIndex
			);
			this->m_collisionFaceCounts.push_back(realizedFaceCount);
		}
	}

	if (facePoints.is_empty()) {
		return;
	}
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(facePoints);
	StaticBody3D* staticBody = Object::cast_to<StaticBody3D>(
		this->create_collision_node_custom_trimesh(shape)
	);
	ERR_FAIL_NULL_MSG(staticBody, "Unable to generate tile collision, failed to create the tile's shape");
	staticBody->set_name(String(this->get_name()) + "_col");

	this->add_child(staticBody, true);
	CollisionShape3D* collisionShape =
		Object::cast_to<CollisionShape3D>(staticBody->get_child(0));
	ERR_FAIL_NULL_MSG(
		collisionShape,
		"Unable to generate tile collision, failed to create its shape node"
	);
	this->m_collisionShape = ObjectID(collisionShape->get_instance_id());
	// Set all the owners
	Node* owner = this->get_owner();
	if (owner == nullptr) return;
	staticBody->set_owner(owner);
	collisionShape->set_owner(owner);
}

void Cesium3DTile::set_tile_collision_enabled(bool enabled) {
	if (this->m_collisionShape.is_null()) {
		return;
	}
	CollisionShape3D* collisionShape = Object::cast_to<CollisionShape3D>(
		ObjectDB::get_instance(this->m_collisionShape)
	);
	if (collisionShape != nullptr) {
		collisionShape->set_disabled(!enabled);
	}
}

void Cesium3DTile::set_shared_image_resources(
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>&& resources
) {
	this->m_sharedImageResources = std::move(resources);
}

void Cesium3DTile::set_shared_model_resource(
	const std::shared_ptr<CesiumGltfSharedModelResource>& resource
) {
	this->m_sharedModelResource = resource;
}

void Cesium3DTile::set_model_metadata(
	const Ref<CesiumModelMetadata>& metadata
) {
	this->m_modelMetadata = metadata;
}

Ref<CesiumModelMetadata> Cesium3DTile::get_model_metadata() const {
	return this->m_modelMetadata;
}

void Cesium3DTile::set_primitive_features(const Array& primitiveFeatures) {
	this->m_primitiveFeatures = primitiveFeatures.duplicate(true);
}

Array Cesium3DTile::get_all_primitive_features() const {
	return this->m_primitiveFeatures.duplicate(true);
}

Ref<CesiumPrimitiveFeatures> Cesium3DTile::get_primitive_features(
	int32_t surfaceIndex
) const {
	if (surfaceIndex < 0 || surfaceIndex >= this->m_primitiveFeatures.size()) {
		return Ref<CesiumPrimitiveFeatures>();
	}
	return this->m_primitiveFeatures[surfaceIndex];
}

Node* Cesium3DTile::create_collision_node_custom_trimesh(
	const Ref<ConcavePolygonShape3D>& shape
) {
	if (shape.is_null()) {
		return nullptr;
	}
	
	StaticBody3D* staticBody = memnew(StaticBody3D);
	CollisionShape3D* collisionShape = memnew(CollisionShape3D);

	collisionShape->set_shape(shape);
	staticBody->add_child(collisionShape, true);
	return staticBody;
}

Dictionary Cesium3DTile::get_metadata_table(int32_t index) const {
	if (this->m_modelMetadata.is_null()) {
		return Dictionary();
	}
	Ref<CesiumPropertyTable> table =
		this->m_modelMetadata->get_property_table(index);
	return table.is_valid() ? table->get_properties() : Dictionary();
}

int32_t Cesium3DTile::get_table_count() const {
	return this->m_modelMetadata.is_valid()
		? this->m_modelMetadata->get_property_table_count()
		: 0;
}

Dictionary Cesium3DTile::get_structural_metadata() const {
	return this->get_metadata_table(0);
}

void Cesium3DTile::set_tile_id(const String& tileId) {
	this->m_tileId = tileId;
	this->set_meta("cesium_tile_id", tileId);
}

const String& Cesium3DTile::get_tile_id() const {
	return this->m_tileId;
}

void Cesium3DTile::set_tile_extras(const Variant& tileExtras) {
	this->m_tileExtras = tileExtras.duplicate(true);
	this->set_meta("cesium_tile_extras", this->m_tileExtras);
}

Variant Cesium3DTile::get_tile_extras() const {
	return this->m_tileExtras.duplicate(true);
}

void Cesium3DTile::set_bounding_volumes(
	const Ref<CesiumBoundingVolume>& tileBounds,
	const Ref<CesiumBoundingVolume>& contentBounds,
	const Ref<CesiumBoundingVolume>& viewerRequestBounds
) {
	this->m_tileBounds = tileBounds;
	this->m_contentBounds = contentBounds;
	this->m_viewerRequestBounds = viewerRequestBounds;
}

Ref<CesiumBoundingVolume> Cesium3DTile::get_tile_bounds() const {
	return this->m_tileBounds;
}

Ref<CesiumBoundingVolume> Cesium3DTile::get_content_bounds() const {
	return this->m_contentBounds;
}

Ref<CesiumBoundingVolume> Cesium3DTile::get_viewer_request_bounds() const {
	return this->m_viewerRequestBounds;
}

AABB Cesium3DTile::get_tile_source_aabb() const {
	return this->m_tileBounds.is_valid()
		? this->m_tileBounds->get_source_aabb()
		: AABB();
}

Array Cesium3DTile::get_loaded_tile_primitives() const {
	Array result;
	for (const Ref<CesiumLoadedTilePrimitive>& primitive : this->m_loadedTilePrimitives) {
		result.push_back(primitive);
	}
	return result;
}

int32_t Cesium3DTile::get_loaded_tile_primitive_count() const {
	return static_cast<int32_t>(this->m_loadedTilePrimitives.size());
}

Ref<CesiumLoadedTilePrimitive> Cesium3DTile::get_loaded_tile_primitive(
	int32_t surfaceIndex
) const {
	if (
		surfaceIndex < 0 ||
		surfaceIndex >= static_cast<int32_t>(this->m_loadedTilePrimitives.size())
	) {
		return Ref<CesiumLoadedTilePrimitive>();
	}
	return this->m_loadedTilePrimitives[surfaceIndex];
}

Ref<CesiumLoadedTilePrimitive>
Cesium3DTile::get_loaded_tile_primitive_for_mesh_surface(
	int32_t meshSurfaceIndex
) const {
	if (
		meshSurfaceIndex < 0 ||
		meshSurfaceIndex >=
			static_cast<int32_t>(this->m_parentSurfaceToPrimitiveIndex.size())
	) {
		return Ref<CesiumLoadedTilePrimitive>();
	}
	return this->get_loaded_tile_primitive(
		this->m_parentSurfaceToPrimitiveIndex[meshSurfaceIndex]
	);
}

Ref<CesiumLoadedTilePrimitive>
Cesium3DTile::get_loaded_tile_primitive_for_collision_face(
	int64_t globalFaceIndex,
	int64_t* localFaceIndex
) const {
	if (globalFaceIndex < 0) {
		return Ref<CesiumLoadedTilePrimitive>();
	}
	int64_t remaining = globalFaceIndex;
	for (size_t rangeIndex = 0;
		rangeIndex < this->m_collisionFaceCounts.size();
		++rangeIndex) {
		const int64_t faceCount = this->m_collisionFaceCounts[rangeIndex];
		if (remaining < faceCount) {
			if (localFaceIndex != nullptr) {
				*localFaceIndex = remaining;
			}
			return this->get_loaded_tile_primitive(
				this->m_collisionFacePrimitiveIndices[rangeIndex]
			);
		}
		remaining -= faceCount;
	}
	return Ref<CesiumLoadedTilePrimitive>();
}

Array Cesium3DTile::get_all_raster_overlay_bindings() const {
	return this->get_raster_overlay_bindings();
}

Array Cesium3DTile::get_raster_overlay_bindings(int32_t surfaceIndex) const {
	Array result;
	for (const Ref<CesiumRasterOverlayBinding>& binding : this->m_rasterOverlayBindings) {
		if (binding.is_null() || binding->get_tile_primitive().is_null()) {
			continue;
		}
		if (
			surfaceIndex < 0 ||
			binding->get_tile_primitive()->get_surface_index() == surfaceIndex
		) {
			result.push_back(binding);
		}
	}
	return result;
}

void Cesium3DTile::set_loaded_tile_primitives(
	const std::vector<Ref<CesiumLoadedTilePrimitive>>& primitives,
	const std::vector<Ref<Material>>& baseSurfaceMaterials
) {
	this->m_loadedTilePrimitives = primitives;
	this->m_baseSurfaceMaterials = baseSurfaceMaterials;
}

void Cesium3DTile::set_primitive_render_nodes(
	const std::vector<GeometryInstance3D*>& renderNodes,
	const std::vector<int32_t>& meshSurfaceIndices,
	const std::vector<int32_t>& parentSurfaceToPrimitiveIndex
) {
	this->m_primitiveRenderNodes.clear();
	this->m_primitiveRenderNodes.reserve(renderNodes.size());
	for (GeometryInstance3D* renderNode : renderNodes) {
		this->m_primitiveRenderNodes.emplace_back(
			renderNode == nullptr
				? ObjectID()
				: ObjectID(renderNode->get_instance_id())
		);
	}
	this->m_primitiveMeshSurfaceIndices = meshSurfaceIndices;
	this->m_parentSurfaceToPrimitiveIndex = parentSurfaceToPrimitiveIndex;
}

GeometryInstance3D* Cesium3DTile::get_primitive_render_node(
	int32_t rendererPrimitiveIndex
) const {
	if (
		rendererPrimitiveIndex < 0 ||
		rendererPrimitiveIndex >=
			static_cast<int32_t>(this->m_primitiveRenderNodes.size()) ||
		this->m_primitiveRenderNodes[rendererPrimitiveIndex].is_null()
	) {
		return nullptr;
	}
	return Object::cast_to<GeometryInstance3D>(ObjectDB::get_instance(
		this->m_primitiveRenderNodes[rendererPrimitiveIndex]
	));
}

int32_t Cesium3DTile::get_primitive_mesh_surface_index(
	int32_t rendererPrimitiveIndex
) const {
	if (
		rendererPrimitiveIndex < 0 ||
		rendererPrimitiveIndex >=
			static_cast<int32_t>(this->m_primitiveMeshSurfaceIndices.size())
	) {
		return -1;
	}
	return this->m_primitiveMeshSurfaceIndices[rendererPrimitiveIndex];
}

void Cesium3DTile::set_primitive_override_material(
	int32_t rendererPrimitiveIndex,
	const Ref<Material>& material
) {
	GeometryInstance3D* renderNode =
		this->get_primitive_render_node(rendererPrimitiveIndex);
	if (renderNode == nullptr) {
		return;
	}
	if (renderNode == this) {
		const int32_t meshSurfaceIndex =
			this->get_primitive_mesh_surface_index(rendererPrimitiveIndex);
		if (meshSurfaceIndex >= 0) {
			this->set_surface_override_material(meshSurfaceIndex, material);
		}
		return;
	}
	renderNode->set_material_override(material);
}

Ref<CesiumRasterOverlayBinding> Cesium3DTile::add_raster_overlay_binding(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	ERR_FAIL_COND_V(binding.is_null(), Ref<CesiumRasterOverlayBinding>());
	Ref<CesiumLoadedTilePrimitive> primitive = binding->get_tile_primitive();
	ERR_FAIL_COND_V(primitive.is_null(), Ref<CesiumRasterOverlayBinding>());

	const int32_t surfaceIndex = primitive->get_surface_index();
	for (size_t index = 0; index < this->m_rasterOverlayBindings.size(); ++index) {
		const Ref<CesiumRasterOverlayBinding>& existing =
			this->m_rasterOverlayBindings[index];
		if (
			existing.is_valid() &&
			existing->get_tile_primitive().is_valid() &&
			existing->get_tile_primitive()->get_surface_index() == surfaceIndex &&
			existing->get_overlay_key() == binding->get_overlay_key()
		) {
			Ref<CesiumRasterOverlayBinding> replaced = existing;
			this->m_rasterOverlayBindings[index] = binding;
			return replaced;
		}
	}

	this->m_rasterOverlayBindings.push_back(binding);
	return Ref<CesiumRasterOverlayBinding>();
}

Ref<CesiumRasterOverlayBinding> Cesium3DTile::remove_raster_overlay_binding(
	int32_t surfaceIndex,
	const String& overlayKey,
	const Ref<Texture2D>& expectedTexture
) {
	for (size_t index = 0; index < this->m_rasterOverlayBindings.size(); ++index) {
		const Ref<CesiumRasterOverlayBinding>& binding =
			this->m_rasterOverlayBindings[index];
		if (
			binding.is_valid() &&
			binding->get_tile_primitive().is_valid() &&
			binding->get_tile_primitive()->get_surface_index() == surfaceIndex &&
			binding->get_overlay_key() == overlayKey &&
			(
				expectedTexture.is_null() ||
				binding->get_texture() == expectedTexture
			)
		) {
			Ref<CesiumRasterOverlayBinding> removed = binding;
			this->m_rasterOverlayBindings.erase(
				this->m_rasterOverlayBindings.begin() + index
			);
			return removed;
		}
	}
	return Ref<CesiumRasterOverlayBinding>();
}

Array Cesium3DTile::take_raster_overlay_bindings() {
	Array result;
	for (const Ref<CesiumRasterOverlayBinding>& binding : this->m_rasterOverlayBindings) {
		result.push_back(binding);
	}
	this->m_rasterOverlayBindings.clear();
	return result;
}

void Cesium3DTile::refresh_raster_overlay_material(int32_t surfaceIndex) {
	if (
		surfaceIndex < 0 ||
		surfaceIndex >= static_cast<int32_t>(this->m_baseSurfaceMaterials.size())
	) {
		return;
	}

	const Ref<Material>& baseMaterial = this->m_baseSurfaceMaterials[surfaceIndex];
	Array bindings = this->get_raster_overlay_bindings(surfaceIndex);
	if (bindings.is_empty()) {
		this->set_primitive_override_material(surfaceIndex, baseMaterial);
		return;
	}

	Ref<Material> material;
	if (baseMaterial.is_valid()) {
		material = baseMaterial->duplicate(true);
	}
	if (material.is_null()) {
		return;
	}
	for (int32_t index = 0; index < bindings.size(); ++index) {
		Ref<CesiumRasterOverlayBinding> binding = bindings[index];
		binding->set_material(material);
	}

	Ref<ShaderMaterial> shaderMaterial = material;
	if (shaderMaterial.is_valid()) {
		for (int32_t index = 0; index < bindings.size(); ++index) {
			Ref<CesiumRasterOverlayBinding> binding = bindings[index];
			binding->apply_to_material(material);
		}
	} else {
		// StandardMaterial3D has one albedo slot. Keep all bindings observable,
		// but display the first one whose coordinates are available in UV.
		Ref<StandardMaterial3D> standardMaterial = material;
		if (standardMaterial.is_valid()) {
			for (int32_t index = 0; index < bindings.size(); ++index) {
				Ref<CesiumRasterOverlayBinding> binding = bindings[index];
				if (binding->get_texture_coordinate_index() != 0) {
					continue;
				}
				const Vector2 scale = binding->get_scale();
				const Vector2 translation = binding->get_translation();
				standardMaterial->set_texture(
					BaseMaterial3D::TEXTURE_ALBEDO,
					binding->get_texture()
				);
				// Cesium raster UVs use a bottom-left image origin; Godot samples
				// textures from the top-left.
				standardMaterial->set_uv1_scale(Vector3(scale.x, -scale.y, 1.0));
				standardMaterial->set_uv1_offset(Vector3(
					translation.x,
					1.0 - translation.y,
					0.0
				));
				break;
			}
		}
	}

	this->set_primitive_override_material(surfaceIndex, material);
}

void Cesium3DTile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_metadata_table", "index"), &Cesium3DTile::get_metadata_table);
	ClassDB::bind_method(D_METHOD("get_model_metadata"), &Cesium3DTile::get_model_metadata);
	ClassDB::bind_method(D_METHOD("get_all_primitive_features"), &Cesium3DTile::get_all_primitive_features);
	ClassDB::bind_method(D_METHOD("get_primitive_features", "surface_index"), &Cesium3DTile::get_primitive_features);
    ClassDB::bind_method(D_METHOD("get_table_count"), &Cesium3DTile::get_table_count);
    ClassDB::bind_method(D_METHOD("generate_tile_collision"), &Cesium3DTile::generate_tile_collision);
	ClassDB::bind_method(D_METHOD("get_structural_metadata"), &Cesium3DTile::get_structural_metadata);
	ClassDB::bind_method(D_METHOD("get_tile_id"), &Cesium3DTile::get_tile_id);
	ClassDB::bind_method(D_METHOD("get_tile_extras"), &Cesium3DTile::get_tile_extras);
	ClassDB::bind_method(D_METHOD("get_tile_bounds"), &Cesium3DTile::get_tile_bounds);
	ClassDB::bind_method(D_METHOD("get_content_bounds"), &Cesium3DTile::get_content_bounds);
	ClassDB::bind_method(D_METHOD("get_viewer_request_bounds"), &Cesium3DTile::get_viewer_request_bounds);
	ClassDB::bind_method(D_METHOD("get_tile_source_aabb"), &Cesium3DTile::get_tile_source_aabb);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_primitives"), &Cesium3DTile::get_loaded_tile_primitives);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_primitive_count"), &Cesium3DTile::get_loaded_tile_primitive_count);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_primitive", "surface_index"), &Cesium3DTile::get_loaded_tile_primitive);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_primitive_for_mesh_surface", "mesh_surface_index"), &Cesium3DTile::get_loaded_tile_primitive_for_mesh_surface);
	ClassDB::bind_method(D_METHOD("get_all_raster_overlay_bindings"), &Cesium3DTile::get_all_raster_overlay_bindings);
	ClassDB::bind_method(D_METHOD("get_raster_overlay_bindings", "surface_index"), &Cesium3DTile::get_raster_overlay_bindings, DEFVAL(-1));
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tile_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_tile_id");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model_metadata", PROPERTY_HINT_RESOURCE_TYPE, "CesiumModelMetadata", PROPERTY_USAGE_NONE), "", "get_model_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "primitive_features", PROPERTY_HINT_ARRAY_TYPE, "CesiumPrimitiveFeatures", PROPERTY_USAGE_NONE), "", "get_all_primitive_features");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "tile_extras", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_tile_extras");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tile_bounds", PROPERTY_HINT_RESOURCE_TYPE, "CesiumBoundingVolume", PROPERTY_USAGE_NONE), "", "get_tile_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "content_bounds", PROPERTY_HINT_RESOURCE_TYPE, "CesiumBoundingVolume", PROPERTY_USAGE_NONE), "", "get_content_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "viewer_request_bounds", PROPERTY_HINT_RESOURCE_TYPE, "CesiumBoundingVolume", PROPERTY_USAGE_NONE), "", "get_viewer_request_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "loaded_tile_primitives", PROPERTY_HINT_ARRAY_TYPE, "CesiumLoadedTilePrimitive", PROPERTY_USAGE_NONE), "", "get_loaded_tile_primitives");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "raster_overlay_bindings", PROPERTY_HINT_ARRAY_TYPE, "CesiumRasterOverlayBinding", PROPERTY_USAGE_NONE), "", "get_all_raster_overlay_bindings");
}
