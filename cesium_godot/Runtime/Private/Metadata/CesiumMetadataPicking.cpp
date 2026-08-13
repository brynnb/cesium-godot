// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumMetadataPicking.h"

#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Renderer/CesiumGltfInstancedComponent.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/main/node.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/node.hpp"
#endif

namespace {
Dictionary failure(const String& reason) {
	Dictionary result;
	result["valid"] = false;
	result["error"] = reason;
	result["feature_id"] = -1;
	result["surface_index"] = -1;
	result["surface_face_index"] = -1;
	result["instance_index"] = -1;
	result["primitive_metadata"] = Ref<CesiumPrimitiveMetadata>();
	result["metadata"] = Dictionary();
	return result;
}

Dictionary resolve_instanced_mesh_feature_hit(
	const Dictionary& hit,
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) {
	Ref<CesiumPrimitiveFeatures> features = primitive->get_primitive_features();
	if (features.is_null()) {
		return failure("instanced_primitive_has_no_features");
	}
	const int64_t faceIndex = hit.get("face_index", -1);
	if (faceIndex < 0) {
		return failure("hit_has_no_face_index");
	}
	if (!hit.has("position")) {
		return failure("hit_has_no_position");
	}
	CesiumGltfInstancedComponent* component =
		Object::cast_to<CesiumGltfInstancedComponent>(
			primitive->get_render_node()
		);
	if (component == nullptr || component->get_multimesh().is_null()) {
		return failure("instanced_render_node_is_not_available");
	}
	const Ref<MultiMesh> multiMesh = component->get_multimesh();
	if (instanceIndex < 0 || instanceIndex >= multiMesh->get_instance_count()) {
		return failure("instance_index_is_out_of_bounds");
	}
	const Vector3 worldPosition = hit.get("position", Vector3());
	const Vector3 componentPosition = component->to_local(worldPosition);
	const Vector3 localPosition = multiMesh->get_instance_transform(
		static_cast<int32_t>(instanceIndex)
	).affine_inverse().xform(componentPosition);
	const int64_t featureId = features->get_feature_id_from_hit(
		faceIndex,
		localPosition,
		featureIdSetIndex
	);
	const Ref<CesiumFeatureIdSet> featureIdSet =
		features->get_feature_id_set(featureIdSetIndex);
	const bool validFeature = featureId >= 0 && featureIdSet.is_valid() &&
		!featureIdSet->is_null_feature_id(featureId);
	const Ref<CesiumModelMetadata> modelMetadata =
		primitive->get_model_metadata();
	Dictionary sourcePlacement;
	sourcePlacement["node_index"] = primitive->get_node_index();
	sourcePlacement["mesh_index"] = primitive->get_mesh_index();
	sourcePlacement["primitive_index"] = primitive->get_primitive_index();
	Dictionary result;
	result["valid"] = validFeature;
	result["error"] = validFeature
		? String()
		: String("feature_id_not_found");
	result["tile"] = primitive->get_loaded_tile();
	result["tile_id"] = primitive->get_tile_id();
	result["tile_extras"] = primitive->get_tile_extras();
	result["primitive"] = primitive;
	result["primitive_features"] = features;
	result["instance_features"] = primitive->get_instance_features();
	result["primitive_metadata"] = primitive->get_primitive_metadata();
	result["model_metadata"] = modelMetadata;
	result["surface_index"] = primitive->get_surface_index();
	result["mesh_surface_index"] = primitive->get_mesh_surface_index();
	result["face_index"] = faceIndex;
	result["surface_face_index"] = faceIndex;
	result["instance_index"] = instanceIndex;
	result["local_position"] = localPosition;
	result["feature_id_set_index"] = featureIdSetIndex;
	result["feature_id"] = featureId;
	result["feature_source"] = "mesh";
	result["metadata"] = features->get_metadata_values_from_hit(
		modelMetadata,
		faceIndex,
		localPosition,
		featureIdSetIndex
	);
	result["source_placement"] = sourcePlacement;
	return result;
}

Cesium3DTile* find_tile(Object* collider) {
	Node* node = Object::cast_to<Node>(collider);
	while (node != nullptr) {
		Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(node);
		if (tile != nullptr) {
			return tile;
		}
		node = node->get_parent();
	}
	return nullptr;
}
}

Dictionary CesiumMetadataPicking::resolve_hit(
	const Dictionary& hit,
	int32_t featureIdSetIndex
) {
	if (hit.is_empty()) {
		return failure("empty_hit");
	}
	Ref<CesiumLoadedTilePrimitive> explicitPrimitive = hit.get(
		"primitive",
		Variant()
	);
	const int64_t explicitInstanceIndex = hit.get("instance_index", -1);
	if (
		explicitPrimitive.is_valid() &&
		explicitPrimitive->is_gpu_instanced() &&
		explicitInstanceIndex >= 0
	) {
		const Ref<CesiumPrimitiveFeatures> instanceFeatures =
			explicitPrimitive->get_instance_features();
		if (
			instanceFeatures.is_valid() &&
			instanceFeatures->get_feature_id_set_count() > 0
		) {
			return resolve_instance(
				explicitPrimitive,
				explicitInstanceIndex,
				featureIdSetIndex
			);
		}
		return resolve_instanced_mesh_feature_hit(
			hit,
			explicitPrimitive,
			explicitInstanceIndex,
			featureIdSetIndex
		);
	}
	const Variant colliderVariant = hit.get("collider", Variant());
	Object* collider = colliderVariant;
	Cesium3DTile* tile = find_tile(collider);
	if (tile == nullptr) {
		return failure("collider_is_not_a_cesium_tile");
	}
	const int64_t globalFaceIndex = hit.get("face_index", -1);
	if (globalFaceIndex < 0) {
		return failure("hit_has_no_face_index");
	}
	if (!hit.has("position")) {
		return failure("hit_has_no_position");
	}

	int64_t surfaceFaceIndex = globalFaceIndex;
	int32_t surfaceIndex = -1;
	int32_t meshSurfaceIndex = -1;
	Ref<CesiumPrimitiveFeatures> features;
	Ref<CesiumLoadedTilePrimitive> primitive =
		tile->get_loaded_tile_primitive_for_collision_face(
			globalFaceIndex,
			&surfaceFaceIndex
		);
	if (primitive.is_valid()) {
		surfaceIndex = primitive->get_surface_index();
		meshSurfaceIndex = primitive->get_mesh_surface_index();
		features = primitive->get_primitive_features();
	}
	if (surfaceIndex < 0 || primitive.is_null()) {
		return failure("face_index_is_out_of_bounds");
	}

	const Vector3 worldPosition = hit.get("position", Vector3());
	const Vector3 localPosition = tile->to_local(worldPosition);
	const int64_t featureId = features.is_valid()
		? features->get_feature_id_from_hit(
			surfaceFaceIndex,
			localPosition,
			featureIdSetIndex
		)
		: -1;
	const Ref<CesiumFeatureIdSet> featureIdSet = features.is_valid()
		? features->get_feature_id_set(featureIdSetIndex)
		: Ref<CesiumFeatureIdSet>();
	const bool validFeature = featureId >= 0 && featureIdSet.is_valid() &&
		!featureIdSet->is_null_feature_id(featureId);
	const Ref<CesiumModelMetadata> modelMetadata = tile->get_model_metadata();
	const Dictionary metadata = features.is_valid()
		? features->get_metadata_values_from_hit(
			modelMetadata,
			surfaceFaceIndex,
			localPosition,
			featureIdSetIndex
		)
		: Dictionary();
	Dictionary sourcePlacement;
	if (primitive.is_valid()) {
		sourcePlacement["node_index"] = primitive->get_node_index();
		sourcePlacement["mesh_index"] = primitive->get_mesh_index();
		sourcePlacement["primitive_index"] = primitive->get_primitive_index();
	}

	Dictionary result;
	result["valid"] = validFeature;
	result["error"] = validFeature ? String() : String("feature_id_not_found");
	result["tile"] = tile;
	result["tile_id"] = tile->get_tile_id();
	result["tile_extras"] = tile->get_tile_extras();
	result["primitive"] = primitive;
	result["primitive_features"] = features;
	result["instance_features"] = Ref<CesiumPrimitiveFeatures>();
	result["primitive_metadata"] = primitive->get_primitive_metadata();
	result["model_metadata"] = modelMetadata;
	result["surface_index"] = surfaceIndex;
	result["mesh_surface_index"] = meshSurfaceIndex;
	result["face_index"] = globalFaceIndex;
	result["surface_face_index"] = surfaceFaceIndex;
	result["instance_index"] = -1;
	result["local_position"] = localPosition;
	result["feature_id_set_index"] = featureIdSetIndex;
	result["feature_id"] = featureId;
	result["feature_source"] = "mesh";
	result["metadata"] = metadata;
	result["source_placement"] = sourcePlacement;
	return result;
}

Dictionary CesiumMetadataPicking::resolve_instance(
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) {
	if (primitive.is_null() || !primitive->is_gpu_instanced()) {
		return failure("primitive_is_not_gpu_instanced");
	}
	if (instanceIndex < 0 || instanceIndex >= primitive->get_instance_count()) {
		return failure("instance_index_is_out_of_bounds");
	}
	const Ref<CesiumPrimitiveFeatures> features =
		primitive->get_instance_features();
	if (features.is_null()) {
		return failure("primitive_has_no_instance_features");
	}
	const Ref<CesiumFeatureIdSet> featureIdSet =
		features->get_feature_id_set(featureIdSetIndex);
	const int64_t featureId = features->get_feature_id_from_instance(
		instanceIndex,
		featureIdSetIndex
	);
	const bool validFeature = featureId >= 0 && featureIdSet.is_valid() &&
		!featureIdSet->is_null_feature_id(featureId);
	const Ref<CesiumModelMetadata> modelMetadata =
		primitive->get_model_metadata();
	Dictionary sourcePlacement;
	sourcePlacement["node_index"] = primitive->get_node_index();
	sourcePlacement["mesh_index"] = primitive->get_mesh_index();
	sourcePlacement["primitive_index"] = primitive->get_primitive_index();
	Dictionary result;
	result["valid"] = validFeature;
	result["error"] = validFeature
		? String()
		: String("feature_id_not_found");
	result["tile"] = primitive->get_loaded_tile();
	result["tile_id"] = primitive->get_tile_id();
	result["tile_extras"] = primitive->get_tile_extras();
	result["primitive"] = primitive;
	result["primitive_features"] = primitive->get_primitive_features();
	result["instance_features"] = features;
	result["primitive_metadata"] = primitive->get_primitive_metadata();
	result["model_metadata"] = modelMetadata;
	result["surface_index"] = primitive->get_surface_index();
	result["mesh_surface_index"] = primitive->get_mesh_surface_index();
	result["face_index"] = -1;
	result["surface_face_index"] = -1;
	result["instance_index"] = instanceIndex;
	result["feature_id_set_index"] = featureIdSetIndex;
	result["feature_id"] = featureId;
	result["feature_source"] = "instance";
	result["metadata"] = features->get_metadata_values_for_instance(
		modelMetadata,
		instanceIndex,
		featureIdSetIndex
	);
	result["source_placement"] = sourcePlacement;
	return result;
}

int64_t CesiumMetadataPicking::get_feature_id_from_instance(
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) {
	return resolve_instance(
		primitive,
		instanceIndex,
		featureIdSetIndex
	).get("feature_id", -1);
}

Dictionary CesiumMetadataPicking::get_metadata_values_from_instance(
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) {
	return resolve_instance(
		primitive,
		instanceIndex,
		featureIdSetIndex
	).get("metadata", Dictionary());
}

int64_t CesiumMetadataPicking::get_feature_id_from_hit(
	const Dictionary& hit,
	int32_t featureIdSetIndex
) {
	return resolve_hit(hit, featureIdSetIndex).get("feature_id", -1);
}

Dictionary CesiumMetadataPicking::get_metadata_values_from_hit(
	const Dictionary& hit,
	int32_t featureIdSetIndex
) {
	return resolve_hit(hit, featureIdSetIndex).get("metadata", Dictionary());
}

Dictionary CesiumMetadataPicking::get_property_texture_values_from_hit(
	const Dictionary& hit,
	int32_t propertyTextureIndex
) {
	Ref<CesiumLoadedTilePrimitive> explicitPrimitive = hit.get(
		"primitive",
		Variant()
	);
	if (
		explicitPrimitive.is_valid() && explicitPrimitive->is_gpu_instanced()
	) {
		const int64_t instanceIndex = hit.get("instance_index", -1);
		const int64_t faceIndex = hit.get("face_index", -1);
		if (
			instanceIndex < 0 ||
			instanceIndex >= explicitPrimitive->get_instance_count() ||
			faceIndex < 0 || !hit.has("position")
		) {
			return Dictionary();
		}
		CesiumGltfInstancedComponent* component =
			Object::cast_to<CesiumGltfInstancedComponent>(
				explicitPrimitive->get_render_node()
			);
		if (component == nullptr || component->get_multimesh().is_null()) {
			return Dictionary();
		}
		const Vector3 componentPosition = component->to_local(
			hit.get("position", Vector3())
		);
		const Vector3 localPosition = component->get_multimesh()
			->get_instance_transform(static_cast<int32_t>(instanceIndex))
			.affine_inverse()
			.xform(componentPosition);
		const Ref<CesiumPrimitiveMetadata> primitiveMetadata =
			explicitPrimitive->get_primitive_metadata();
		return primitiveMetadata.is_valid()
			? primitiveMetadata->get_property_texture_values_from_hit(
				explicitPrimitive->get_model_metadata(),
				propertyTextureIndex,
				faceIndex,
				localPosition
			)
			: Dictionary();
	}

	const Dictionary context = resolve_hit(hit);
	Ref<CesiumLoadedTilePrimitive> primitive = context.get(
		"primitive",
		Variant()
	);
	if (primitive.is_null()) {
		return Dictionary();
	}
	const Ref<CesiumPrimitiveMetadata> primitiveMetadata =
		primitive->get_primitive_metadata();
	return primitiveMetadata.is_valid()
		? primitiveMetadata->get_property_texture_values_from_hit(
			primitive->get_model_metadata(),
			propertyTextureIndex,
			context.get("surface_face_index", -1),
			context.get("local_position", Vector3())
		)
		: Dictionary();
}

void CesiumMetadataPicking::_bind_methods() {
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("resolve_hit", "hit", "feature_id_set_index"), &CesiumMetadataPicking::resolve_hit, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("resolve_instance", "primitive", "instance_index", "feature_id_set_index"), &CesiumMetadataPicking::resolve_instance, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("get_feature_id_from_instance", "primitive", "instance_index", "feature_id_set_index"), &CesiumMetadataPicking::get_feature_id_from_instance, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("get_metadata_values_from_instance", "primitive", "instance_index", "feature_id_set_index"), &CesiumMetadataPicking::get_metadata_values_from_instance, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("get_feature_id_from_hit", "hit", "feature_id_set_index"), &CesiumMetadataPicking::get_feature_id_from_hit, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("get_metadata_values_from_hit", "hit", "feature_id_set_index"), &CesiumMetadataPicking::get_metadata_values_from_hit, DEFVAL(0));
	ClassDB::bind_static_method("CesiumMetadataPicking", D_METHOD("get_property_texture_values_from_hit", "hit", "property_texture_index"), &CesiumMetadataPicking::get_property_texture_values_from_hit);
}
