// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumFeatureIdSet.h"

#include "Runtime/Private/Metadata/CesiumPrimitiveFeaturesSnapshot.h"

#include <CesiumGltf/TextureView.h>

namespace {
int64_t sample_texture_feature_id(
	const CesiumFeatureIdSetSnapshot& snapshot,
	double u,
	double v
) {
	if (
		snapshot.type != CesiumFeatureIdSetType::Texture ||
		snapshot.status != CesiumFeatureIdSetStatus::Valid ||
		!snapshot.textureView || snapshot.textureChannels.empty() ||
		snapshot.textureView->getTextureViewStatus() !=
			CesiumGltf::TextureViewStatus::Valid
	) {
		return -1;
	}
	if (snapshot.textureTransform) {
		const glm::dvec2 transformed =
			snapshot.textureTransform->applyTransform(u, v);
		u = transformed.x;
		v = transformed.y;
	}
	const std::vector<uint8_t> sample =
		snapshot.textureView->sampleNearestPixel(
			u,
			v,
			snapshot.textureChannels
		);
	int64_t featureId = 0;
	for (size_t index = 0; index < sample.size(); ++index) {
		featureId |= static_cast<int64_t>(sample[index]) << (index * 8);
	}
	return featureId;
}
}

void CesiumFeatureIdSet::initialize(
	const std::shared_ptr<const CesiumFeatureIdSetSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

int32_t CesiumFeatureIdSet::get_feature_id_set_type() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->type)
		: static_cast<int32_t>(CesiumFeatureIdSetType::None);
}

String CesiumFeatureIdSet::get_feature_id_set_type_name() const {
	switch (static_cast<CesiumFeatureIdSetType>(
		this->get_feature_id_set_type()
	)) {
	case CesiumFeatureIdSetType::Attribute:
		return "attribute";
	case CesiumFeatureIdSetType::Texture:
		return "texture";
	case CesiumFeatureIdSetType::Implicit:
		return "implicit";
	case CesiumFeatureIdSetType::Instance:
		return "instance";
	case CesiumFeatureIdSetType::InstanceImplicit:
		return "instance_implicit";
	case CesiumFeatureIdSetType::None:
		return "none";
	}
	return "none";
}

int32_t CesiumFeatureIdSet::get_status() const {
	return this->m_snapshot
		? static_cast<int32_t>(this->m_snapshot->status)
		: static_cast<int32_t>(
			CesiumFeatureIdSetStatus::ErrorInvalidDefinition
		);
}

String CesiumFeatureIdSet::get_status_name() const {
	switch (static_cast<CesiumFeatureIdSetStatus>(this->get_status())) {
	case CesiumFeatureIdSetStatus::Valid:
		return "valid";
	case CesiumFeatureIdSetStatus::ErrorInvalidAccessor:
		return "error_invalid_accessor";
	case CesiumFeatureIdSetStatus::ErrorInvalidTexture:
		return "error_invalid_texture";
	case CesiumFeatureIdSetStatus::ErrorInvalidDefinition:
		return "error_invalid_definition";
	}
	return "error_invalid_definition";
}

bool CesiumFeatureIdSet::is_valid() const {
	return this->m_snapshot &&
		this->m_snapshot->status == CesiumFeatureIdSetStatus::Valid;
}

int64_t CesiumFeatureIdSet::get_feature_count() const {
	return this->m_snapshot ? this->m_snapshot->featureCount : 0;
}

int64_t CesiumFeatureIdSet::get_null_feature_id() const {
	return this->m_snapshot ? this->m_snapshot->nullFeatureId : -1;
}

int32_t CesiumFeatureIdSet::get_property_table_index() const {
	return this->m_snapshot ? this->m_snapshot->propertyTableIndex : -1;
}

String CesiumFeatureIdSet::get_label() const {
	return this->m_snapshot ? String(this->m_snapshot->label.c_str()) : String();
}

int64_t CesiumFeatureIdSet::get_attribute_index() const {
	return this->m_snapshot ? this->m_snapshot->attributeIndex : -1;
}

int64_t CesiumFeatureIdSet::get_texture_coordinate_set_index() const {
	return this->m_snapshot
		? this->m_snapshot->textureCoordinateSetIndex
		: -1;
}

int64_t CesiumFeatureIdSet::get_vertex_count() const {
	return this->m_snapshot
		? static_cast<int64_t>(this->m_snapshot->vertexFeatureIds.size())
		: 0;
}

int64_t CesiumFeatureIdSet::get_instance_count() const {
	return this->m_snapshot ? this->m_snapshot->instanceCount : 0;
}

int64_t CesiumFeatureIdSet::get_feature_id_for_vertex(
	int64_t vertexIndex
) const {
	if (!this->is_valid() || vertexIndex < 0) {
		return -1;
	}
	if (this->m_snapshot->type == CesiumFeatureIdSetType::Implicit) {
		return vertexIndex < this->m_snapshot->featureCount ? vertexIndex : -1;
	}
	if (
		vertexIndex >=
		static_cast<int64_t>(this->m_snapshot->vertexFeatureIds.size())
	) {
		return -1;
	}
	return this->m_snapshot->vertexFeatureIds[
		static_cast<size_t>(vertexIndex)
	];
}

int64_t CesiumFeatureIdSet::get_feature_id_for_instance(
	int64_t instanceIndex
) const {
	if (
		!this->is_valid() || instanceIndex < 0 ||
		instanceIndex >= this->get_instance_count()
	) {
		return -1;
	}
	if (
		this->m_snapshot->type == CesiumFeatureIdSetType::InstanceImplicit
	) {
		return instanceIndex;
	}
	if (
		this->m_snapshot->type != CesiumFeatureIdSetType::Instance ||
		instanceIndex >= static_cast<int64_t>(
			this->m_snapshot->instanceFeatureIds.size()
		)
	) {
		return -1;
	}
	return this->m_snapshot->instanceFeatureIds[
		static_cast<size_t>(instanceIndex)
	];
}

int64_t CesiumFeatureIdSet::get_feature_id_for_texture_coordinate(
	const Vector2& textureCoordinate
) const {
	return this->m_snapshot
		? sample_texture_feature_id(
			*this->m_snapshot,
			textureCoordinate.x,
			textureCoordinate.y
		)
		: -1;
}

bool CesiumFeatureIdSet::is_null_feature_id(int64_t featureId) const {
	return this->m_snapshot && this->m_snapshot->nullFeatureId >= 0 &&
		featureId == this->m_snapshot->nullFeatureId;
}

void CesiumFeatureIdSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_feature_id_set_type"), &CesiumFeatureIdSet::get_feature_id_set_type);
	ClassDB::bind_method(D_METHOD("get_feature_id_set_type_name"), &CesiumFeatureIdSet::get_feature_id_set_type_name);
	ClassDB::bind_method(D_METHOD("get_status"), &CesiumFeatureIdSet::get_status);
	ClassDB::bind_method(D_METHOD("get_status_name"), &CesiumFeatureIdSet::get_status_name);
	ClassDB::bind_method(D_METHOD("is_valid"), &CesiumFeatureIdSet::is_valid);
	ClassDB::bind_method(D_METHOD("get_feature_count"), &CesiumFeatureIdSet::get_feature_count);
	ClassDB::bind_method(D_METHOD("get_null_feature_id"), &CesiumFeatureIdSet::get_null_feature_id);
	ClassDB::bind_method(D_METHOD("get_property_table_index"), &CesiumFeatureIdSet::get_property_table_index);
	ClassDB::bind_method(D_METHOD("get_label"), &CesiumFeatureIdSet::get_label);
	ClassDB::bind_method(D_METHOD("get_attribute_index"), &CesiumFeatureIdSet::get_attribute_index);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_set_index"), &CesiumFeatureIdSet::get_texture_coordinate_set_index);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &CesiumFeatureIdSet::get_vertex_count);
	ClassDB::bind_method(D_METHOD("get_instance_count"), &CesiumFeatureIdSet::get_instance_count);
	ClassDB::bind_method(D_METHOD("get_feature_id_for_vertex", "vertex_index"), &CesiumFeatureIdSet::get_feature_id_for_vertex);
	ClassDB::bind_method(D_METHOD("get_feature_id_for_instance", "instance_index"), &CesiumFeatureIdSet::get_feature_id_for_instance);
	ClassDB::bind_method(D_METHOD("get_feature_id_for_texture_coordinate", "texture_coordinate"), &CesiumFeatureIdSet::get_feature_id_for_texture_coordinate);
	ClassDB::bind_method(D_METHOD("is_null_feature_id", "feature_id"), &CesiumFeatureIdSet::is_null_feature_id);

	BIND_ENUM_CONSTANT(None);
	BIND_ENUM_CONSTANT(Attribute);
	BIND_ENUM_CONSTANT(Texture);
	BIND_ENUM_CONSTANT(Implicit);
	BIND_ENUM_CONSTANT(Instance);
	BIND_ENUM_CONSTANT(InstanceImplicit);
	BIND_ENUM_CONSTANT(Valid);
	BIND_ENUM_CONSTANT(ErrorInvalidDefinition);
	BIND_ENUM_CONSTANT(ErrorInvalidAccessor);
	BIND_ENUM_CONSTANT(ErrorInvalidTexture);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "feature_id_set_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_feature_id_set_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "feature_id_set_type_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_feature_id_set_type_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "status", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "status_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_status_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "feature_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_feature_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "null_feature_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_null_feature_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "property_table_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_table_index");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "attribute_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_attribute_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_coordinate_set_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_coordinate_set_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertex_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_vertex_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "instance_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_instance_count");
}
