// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"

#include "Runtime/Private/Metadata/CesiumPrimitiveFeaturesSnapshot.h"

#include <glm/geometric.hpp>

namespace {
bool find_texture_coordinate_at_hit(
	const CesiumPrimitiveFeaturesSnapshot& primitive,
	const CesiumFeatureIdSetSnapshot& featureSet,
	int64_t faceIndex,
	const Vector3& localPosition,
	Vector2* result
) {
	if (
		result == nullptr || faceIndex < 0 ||
		faceIndex >= static_cast<int64_t>(primitive.triangleIndices.size() / 3) ||
		primitive.vertexPositions.size() !=
			static_cast<size_t>(primitive.vertexCount) ||
		featureSet.vertexTextureCoordinates.size() !=
			static_cast<size_t>(primitive.vertexCount)
	) {
		return false;
	}
	const size_t first = static_cast<size_t>(faceIndex * 3);
	const int64_t index0 = primitive.triangleIndices[first];
	const int64_t index1 = primitive.triangleIndices[first + 1];
	const int64_t index2 = primitive.triangleIndices[first + 2];
	if (
		index0 < 0 || index1 < 0 || index2 < 0 ||
		index0 >= primitive.vertexCount || index1 >= primitive.vertexCount ||
		index2 >= primitive.vertexCount
	) {
		return false;
	}
	const glm::dvec3& a = primitive.vertexPositions[static_cast<size_t>(index0)];
	const glm::dvec3& b = primitive.vertexPositions[static_cast<size_t>(index1)];
	const glm::dvec3& c = primitive.vertexPositions[static_cast<size_t>(index2)];
	const glm::dvec3 point(
		localPosition.x,
		localPosition.y,
		localPosition.z
	);
	const glm::dvec3 edge0 = b - a;
	const glm::dvec3 edge1 = c - a;
	const glm::dvec3 offset = point - a;
	const double dot00 = glm::dot(edge0, edge0);
	const double dot01 = glm::dot(edge0, edge1);
	const double dot11 = glm::dot(edge1, edge1);
	const double dot20 = glm::dot(offset, edge0);
	const double dot21 = glm::dot(offset, edge1);
	const double denominator = dot00 * dot11 - dot01 * dot01;
	if (Math::abs(denominator) <= 1e-18) {
		return false;
	}
	const double weight1 = (dot11 * dot20 - dot01 * dot21) / denominator;
	const double weight2 = (dot00 * dot21 - dot01 * dot20) / denominator;
	const double weight0 = 1.0 - weight1 - weight2;
	const glm::dvec2 textureCoordinate =
		weight0 * featureSet.vertexTextureCoordinates[
			static_cast<size_t>(index0)
		] +
		weight1 * featureSet.vertexTextureCoordinates[
			static_cast<size_t>(index1)
		] +
		weight2 * featureSet.vertexTextureCoordinates[
			static_cast<size_t>(index2)
		];
	*result = Vector2(textureCoordinate.x, textureCoordinate.y);
	return true;
}

Dictionary get_metadata_values(
	const Ref<CesiumModelMetadata>& modelMetadata,
	const Ref<CesiumFeatureIdSet>& set,
	int64_t featureId
) {
	if (
		modelMetadata.is_null() || set.is_null() || !set->is_valid() ||
		featureId < 0 || set->is_null_feature_id(featureId)
	) {
		return Dictionary();
	}
	Ref<CesiumPropertyTable> table = modelMetadata->get_property_table(
		set->get_property_table_index()
	);
	return table.is_valid()
		? table->get_metadata_values_for_feature(featureId)
		: Dictionary();
}
}

void CesiumPrimitiveFeatures::initialize(
	const std::shared_ptr<const CesiumPrimitiveFeaturesSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

void CesiumPrimitiveFeatures::add_feature_id_set(
	const Ref<CesiumFeatureIdSet>& featureIdSet
) {
	if (featureIdSet.is_valid()) {
		this->m_featureIdSets.push_back(featureIdSet);
	}
}

Array CesiumPrimitiveFeatures::get_feature_id_sets() const {
	return this->m_featureIdSets.duplicate(true);
}

int32_t CesiumPrimitiveFeatures::get_feature_id_set_count() const {
	return this->m_featureIdSets.size();
}

Ref<CesiumFeatureIdSet> CesiumPrimitiveFeatures::get_feature_id_set(
	int32_t index
) const {
	if (index < 0 || index >= this->m_featureIdSets.size()) {
		return Ref<CesiumFeatureIdSet>();
	}
	return this->m_featureIdSets[index];
}

int64_t CesiumPrimitiveFeatures::get_vertex_count() const {
	return this->m_snapshot ? this->m_snapshot->vertexCount : 0;
}

int64_t CesiumPrimitiveFeatures::get_instance_count() const {
	return this->m_snapshot ? this->m_snapshot->instanceCount : 0;
}

int64_t CesiumPrimitiveFeatures::get_face_count() const {
	return this->m_snapshot
		? static_cast<int64_t>(this->m_snapshot->triangleIndices.size() / 3)
		: 0;
}

int64_t CesiumPrimitiveFeatures::get_first_vertex_from_face(
	int64_t faceIndex
) const {
	if (!this->m_snapshot || faceIndex < 0 || faceIndex >= this->get_face_count()) {
		return -1;
	}
	return this->m_snapshot->triangleIndices[
		static_cast<size_t>(faceIndex * 3)
	];
}

int64_t CesiumPrimitiveFeatures::get_feature_id_from_face(
	int64_t faceIndex,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	if (set.is_null()) {
		return -1;
	}
	return set->get_feature_id_for_vertex(
		this->get_first_vertex_from_face(faceIndex)
	);
}

int64_t CesiumPrimitiveFeatures::get_feature_id_from_hit(
	int64_t faceIndex,
	const Vector3& localPosition,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	if (set.is_null() || !set->is_valid()) {
		return -1;
	}
	if (
		set->get_feature_id_set_type() != CesiumFeatureIdSet::Texture
	) {
		return this->get_feature_id_from_face(faceIndex, featureIdSetIndex);
	}
	if (
		!this->m_snapshot || featureIdSetIndex < 0 ||
		featureIdSetIndex >=
			static_cast<int32_t>(this->m_snapshot->featureIdSets.size())
	) {
		return -1;
	}
	Vector2 textureCoordinate;
	if (!find_texture_coordinate_at_hit(
		*this->m_snapshot,
		*this->m_snapshot->featureIdSets[
			static_cast<size_t>(featureIdSetIndex)
		],
		faceIndex,
		localPosition,
		&textureCoordinate
	)) {
		return -1;
	}
	return set->get_feature_id_for_texture_coordinate(textureCoordinate);
}

int64_t CesiumPrimitiveFeatures::get_feature_id_from_instance(
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	return set.is_valid()
		? set->get_feature_id_for_instance(instanceIndex)
		: -1;
}

Dictionary CesiumPrimitiveFeatures::get_metadata_values_for_face(
	const Ref<CesiumModelMetadata>& modelMetadata,
	int64_t faceIndex,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	const int64_t featureId = this->get_feature_id_from_face(
		faceIndex,
		featureIdSetIndex
	);
	return get_metadata_values(modelMetadata, set, featureId);
}

Dictionary CesiumPrimitiveFeatures::get_metadata_values_from_hit(
	const Ref<CesiumModelMetadata>& modelMetadata,
	int64_t faceIndex,
	const Vector3& localPosition,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	const int64_t featureId = this->get_feature_id_from_hit(
		faceIndex,
		localPosition,
		featureIdSetIndex
	);
	return get_metadata_values(modelMetadata, set, featureId);
}

Dictionary CesiumPrimitiveFeatures::get_metadata_values_for_instance(
	const Ref<CesiumModelMetadata>& modelMetadata,
	int64_t instanceIndex,
	int32_t featureIdSetIndex
) const {
	Ref<CesiumFeatureIdSet> set = this->get_feature_id_set(featureIdSetIndex);
	const int64_t featureId = this->get_feature_id_from_instance(
		instanceIndex,
		featureIdSetIndex
	);
	return get_metadata_values(modelMetadata, set, featureId);
}

void CesiumPrimitiveFeatures::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_feature_id_sets"), &CesiumPrimitiveFeatures::get_feature_id_sets);
	ClassDB::bind_method(D_METHOD("get_feature_id_set_count"), &CesiumPrimitiveFeatures::get_feature_id_set_count);
	ClassDB::bind_method(D_METHOD("get_feature_id_set", "index"), &CesiumPrimitiveFeatures::get_feature_id_set);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &CesiumPrimitiveFeatures::get_vertex_count);
	ClassDB::bind_method(D_METHOD("get_instance_count"), &CesiumPrimitiveFeatures::get_instance_count);
	ClassDB::bind_method(D_METHOD("get_face_count"), &CesiumPrimitiveFeatures::get_face_count);
	ClassDB::bind_method(D_METHOD("get_first_vertex_from_face", "face_index"), &CesiumPrimitiveFeatures::get_first_vertex_from_face);
	ClassDB::bind_method(D_METHOD("get_feature_id_from_face", "face_index", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_feature_id_from_face, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_feature_id_from_hit", "face_index", "local_position", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_feature_id_from_hit, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_feature_id_from_instance", "instance_index", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_feature_id_from_instance, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_metadata_values_for_face", "model_metadata", "face_index", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_metadata_values_for_face, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_metadata_values_from_hit", "model_metadata", "face_index", "local_position", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_metadata_values_from_hit, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_metadata_values_for_instance", "model_metadata", "instance_index", "feature_id_set_index"), &CesiumPrimitiveFeatures::get_metadata_values_for_instance, DEFVAL(0));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "feature_id_sets", PROPERTY_HINT_ARRAY_TYPE, "CesiumFeatureIdSet", PROPERTY_USAGE_NONE), "", "get_feature_id_sets");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertex_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_vertex_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "instance_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_instance_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "face_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_face_count");
}
