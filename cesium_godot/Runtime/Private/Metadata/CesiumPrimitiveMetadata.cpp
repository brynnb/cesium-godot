// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Metadata/CesiumPrimitiveMetadata.h"

#include "Runtime/Private/Metadata/CesiumPrimitiveMetadataSnapshot.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "Runtime/Public/Metadata/CesiumPropertyTexture.h"

#include <glm/geometric.hpp>

#include <algorithm>

namespace {
bool interpolate_texture_coordinate(
	const CesiumPrimitiveMetadataSnapshot& primitive,
	int64_t faceIndex,
	const Vector3& localPosition,
	int64_t textureCoordinateSetIndex,
	Vector2* result
) {
	if (
		result == nullptr || faceIndex < 0 ||
		faceIndex >= static_cast<int64_t>(primitive.triangleIndices.size() / 3) ||
		primitive.vertexPositions.empty()
	) {
		return false;
	}
	const auto coordinateIt = primitive.textureCoordinates.find(
		textureCoordinateSetIndex
	);
	if (
		coordinateIt == primitive.textureCoordinates.end() ||
		coordinateIt->second.size() != primitive.vertexPositions.size()
	) {
		return false;
	}
	const size_t first = static_cast<size_t>(faceIndex * 3);
	const int64_t index0 = primitive.triangleIndices[first];
	const int64_t index1 = primitive.triangleIndices[first + 1];
	const int64_t index2 = primitive.triangleIndices[first + 2];
	if (
		index0 < 0 || index1 < 0 || index2 < 0 ||
		index0 >= static_cast<int64_t>(primitive.vertexPositions.size()) ||
		index1 >= static_cast<int64_t>(primitive.vertexPositions.size()) ||
		index2 >= static_cast<int64_t>(primitive.vertexPositions.size())
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
	const std::vector<glm::dvec2>& coordinates = coordinateIt->second;
	const glm::dvec2 uv =
		weight0 * coordinates[static_cast<size_t>(index0)] +
		weight1 * coordinates[static_cast<size_t>(index1)] +
		weight2 * coordinates[static_cast<size_t>(index2)];
	*result = Vector2(uv.x, uv.y);
	return true;
}
}

void CesiumPrimitiveMetadata::initialize(
	const std::shared_ptr<const CesiumPrimitiveMetadataSnapshot>& snapshot
) {
	this->m_snapshot = snapshot;
}

void CesiumPrimitiveMetadata::add_property_attribute(
	const Ref<CesiumPropertyAttribute>& attribute
) {
	if (attribute.is_valid()) {
		this->m_propertyAttributes.push_back(attribute);
	}
}

Array CesiumPrimitiveMetadata::get_property_texture_indices() const {
	Array result;
	if (this->m_snapshot) {
		for (int32_t index : this->m_snapshot->propertyTextureIndices) {
			result.push_back(index);
		}
	}
	return result;
}

Array CesiumPrimitiveMetadata::get_property_attribute_indices() const {
	Array result;
	if (this->m_snapshot) {
		for (int32_t index : this->m_snapshot->propertyAttributeIndices) {
			result.push_back(index);
		}
	}
	return result;
}

Array CesiumPrimitiveMetadata::get_property_attributes() const {
	return this->m_propertyAttributes.duplicate(true);
}

int32_t CesiumPrimitiveMetadata::get_property_attribute_count() const {
	return this->m_propertyAttributes.size();
}

Ref<CesiumPropertyAttribute> CesiumPrimitiveMetadata::get_property_attribute(
	int32_t index
) const {
	if (index < 0 || index >= this->m_propertyAttributes.size()) {
		return Ref<CesiumPropertyAttribute>();
	}
	return this->m_propertyAttributes[index];
}

Variant CesiumPrimitiveMetadata::find_texture_coordinate_from_hit(
	int64_t faceIndex,
	const Vector3& localPosition,
	int64_t textureCoordinateSetIndex
) const {
	if (!this->m_snapshot) {
		return Variant();
	}
	Vector2 result;
	return interpolate_texture_coordinate(
		*this->m_snapshot,
		faceIndex,
		localPosition,
		textureCoordinateSetIndex,
		&result
	) ? Variant(result) : Variant();
}

Dictionary CesiumPrimitiveMetadata::get_property_texture_values_from_hit(
	const Ref<CesiumModelMetadata>& modelMetadata,
	int32_t propertyTextureIndex,
	int64_t faceIndex,
	const Vector3& localPosition
) const {
	Dictionary result;
	if (
		modelMetadata.is_null() || !this->m_snapshot ||
		std::find(
			this->m_snapshot->propertyTextureIndices.begin(),
			this->m_snapshot->propertyTextureIndices.end(),
			propertyTextureIndex
		) == this->m_snapshot->propertyTextureIndices.end()
	) {
		return result;
	}
	Ref<CesiumPropertyTexture> texture = modelMetadata->get_property_texture(
		propertyTextureIndex
	);
	if (texture.is_null() || !texture->is_valid()) {
		return result;
	}
	const Array properties = texture->get_properties();
	for (int32_t index = 0; index < properties.size(); ++index) {
		Ref<CesiumPropertyTextureProperty> property = properties[index];
		if (property.is_null() || !property->is_valid()) {
			continue;
		}
		if (
			property->get_status() ==
			CesiumPropertyTextureProperty::EmptyPropertyWithDefault
		) {
			result[property->get_property_id()] = property->get_value(Vector2());
			continue;
		}
		const Variant uv = this->find_texture_coordinate_from_hit(
			faceIndex,
			localPosition,
			property->get_texture_coordinate_set_index()
		);
		if (uv.get_type() == Variant::VECTOR2) {
			result[property->get_property_id()] = property->get_value(uv);
		}
	}
	return result;
}

int64_t CesiumPrimitiveMetadata::get_retained_memory_bytes() const {
	return this->m_snapshot
		? static_cast<int64_t>(this->m_snapshot->memoryBytes)
		: 0;
}

void CesiumPrimitiveMetadata::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_property_texture_indices"), &CesiumPrimitiveMetadata::get_property_texture_indices);
	ClassDB::bind_method(D_METHOD("get_property_attribute_indices"), &CesiumPrimitiveMetadata::get_property_attribute_indices);
	ClassDB::bind_method(D_METHOD("get_property_attributes"), &CesiumPrimitiveMetadata::get_property_attributes);
	ClassDB::bind_method(D_METHOD("get_property_attribute_count"), &CesiumPrimitiveMetadata::get_property_attribute_count);
	ClassDB::bind_method(D_METHOD("get_property_attribute", "index"), &CesiumPrimitiveMetadata::get_property_attribute);
	ClassDB::bind_method(D_METHOD("find_texture_coordinate_from_hit", "face_index", "local_position", "texture_coordinate_set_index"), &CesiumPrimitiveMetadata::find_texture_coordinate_from_hit);
	ClassDB::bind_method(D_METHOD("get_property_texture_values_from_hit", "model_metadata", "property_texture_index", "face_index", "local_position"), &CesiumPrimitiveMetadata::get_property_texture_values_from_hit);
	ClassDB::bind_method(D_METHOD("get_retained_memory_bytes"), &CesiumPrimitiveMetadata::get_retained_memory_bytes);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "property_texture_indices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_texture_indices");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "property_attribute_indices", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_property_attribute_indices");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "property_attributes", PROPERTY_HINT_ARRAY_TYPE, "CesiumPropertyAttribute", PROPERTY_USAGE_NONE), "", "get_property_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "retained_memory_bytes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_retained_memory_bytes");
}
