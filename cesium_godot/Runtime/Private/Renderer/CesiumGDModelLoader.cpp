// Mesh/accessor realization in this file is adapted from Cesium for Unreal
// v2.29.0 CesiumGltfComponent / CesiumGltfPrimitiveComponent (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Private/Renderer/CesiumGDModelLoader.h"

#include "Runtime/Private/Materials/CesiumGltfMaterialLoader.h"
#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"
#include "Utils/CesiumMathUtils.h"
#include "error_names.hpp"
#include "missing_functions.hpp"

#include "CesiumGeometry/Transforms.h"
#include "CesiumGltf/Accessor.h"
#include "CesiumGltf/AccessorUtility.h"
#include "CesiumGltf/AccessorView.h"
#include "CesiumGltf/ExtensionCesiumRTC.h"
#include "CesiumGltf/ExtensionBentleyMaterialsPointStyle.h"
#include "CesiumGltf/ExtensionExtMeshGpuInstancing.h"
#include "CesiumGltf/ExtensionExtMeshFeatures.h"
#include "CesiumGltf/ExtensionKhrMaterialsUnlit.h"
#include "CesiumGltf/ExtensionKhrTextureTransform.h"
#include "CesiumGltf/ExtensionMeshPrimitiveExtStructuralMetadata.h"
#include "CesiumImage/ImageAsset.h"
#include "CesiumGltf/KhrTextureTransform.h"
#include "third_party/mikktspace/mikktspace.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/base_material3d.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/packed_color_array.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "scene/resources/material.h"
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace {
const CesiumGltf::Accessor* get_attribute_accessor(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	const std::string& semantic
) {
	auto attribute = primitive.attributes.find(semantic);
	if (attribute == primitive.attributes.end()) {
		return nullptr;
	}
	return CesiumGltf::Model::getSafe(&model.accessors, attribute->second);
}

template <typename T>
double normalized_unsigned(T value) {
	return static_cast<double>(value) /
		static_cast<double>(std::numeric_limits<T>::max());
}

bool is_triangle_mode(int32_t mode) {
	return mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLES ||
		mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP ||
		mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN;
}

Vector3 compute_dimensions(const Vector<Vector3>& vertices) {
	if (vertices.is_empty()) {
		return Vector3();
	}
	AABB bounds(vertices[0], Vector3());
	for (int32_t index = 1; index < vertices.size(); ++index) {
		bounds.expand_to(vertices[index]);
	}
	return bounds.size;
}

int64_t get_bentley_point_diameter(const CesiumGltf::Material* material) {
	if (material == nullptr) {
		return 0;
	}
	const CesiumGltf::ExtensionBentleyMaterialsPointStyle* extension =
		material->getExtension<CesiumGltf::ExtensionBentleyMaterialsPointStyle>();
	if (extension == nullptr) {
		return 0;
	}
	return std::max<int64_t>(0, extension->diameter);
}

template <typename T>
void expand_attribute_by_indices(
	Vector<T>* values,
	const Vector<int32_t>& sourceIndices
) {
	if (values == nullptr || values->is_empty()) {
		return;
	}
	const Vector<T> source = *values;
	values->resize(sourceIndices.size());
	for (int32_t i = 0; i < sourceIndices.size(); ++i) {
		values->set(i, source[sourceIndices[i]]);
	}
}

void expand_triangle_vertices(
	Vector<Vector3>* vertices,
	Vector<Vector3>* normals,
	Vector<Vector4>* tangents,
	Vector<Color>* colors,
	Vector<Vector2>* uv0,
	Vector<Vector2>* uv1,
	Vector<Vector3>* featureIds,
	Vector<int32_t>* indices
) {
	const Vector<int32_t> sourceIndices = *indices;
	expand_attribute_by_indices(vertices, sourceIndices);
	expand_attribute_by_indices(normals, sourceIndices);
	expand_attribute_by_indices(tangents, sourceIndices);
	expand_attribute_by_indices(colors, sourceIndices);
	expand_attribute_by_indices(uv0, sourceIndices);
	expand_attribute_by_indices(uv1, sourceIndices);
	expand_attribute_by_indices(featureIds, sourceIndices);
	indices->resize(sourceIndices.size());
	for (int32_t i = 0; i < indices->size(); ++i) {
		indices->set(i, i);
	}
}

std::string get_feature_id_set_name(
	const CesiumFeatureIdSetSnapshot& set,
	int32_t* featureTextureCounter
) {
	if (!set.label.empty()) {
		return set.label;
	}
	switch (set.type) {
	case CesiumFeatureIdSetType::Attribute:
		return "_FEATURE_ID_" + std::to_string(set.attributeIndex);
	case CesiumFeatureIdSetType::Texture: {
		const int32_t index = featureTextureCounter == nullptr
			? 0
			: (*featureTextureCounter)++;
		return "_FEATURE_ID_TEXTURE_" + std::to_string(index);
	}
	case CesiumFeatureIdSetType::Implicit:
		return "_IMPLICIT_FEATURE_ID";
	case CesiumFeatureIdSetType::Instance:
		return "_FEATURE_INSTANCE_ID_" + std::to_string(set.attributeIndex);
	case CesiumFeatureIdSetType::InstanceImplicit:
		return "_IMPLICIT_FEATURE_INSTANCE_ID";
	case CesiumFeatureIdSetType::None:
		break;
	}
	return std::string();
}

Vector3 encode_feature_id(int64_t featureId, bool valid) {
	if (
		!valid || featureId < 0 ||
		static_cast<uint64_t>(featureId) >
			static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
	) {
		return Vector3(0.0, 0.0, 0.0);
	}
	const uint32_t id = static_cast<uint32_t>(featureId);
	return Vector3(
		static_cast<real_t>(id & 0xffffU),
		static_cast<real_t>((id >> 16U) & 0xffffU),
		1.0
	);
}

CesiumFeatureStyleEncoding prepare_feature_style_encoding(
	const CesiumFeatureStyleEncodingDescription& description,
	const std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>& meshFeatures,
	const std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>& instanceFeatures,
	Vector<Vector3>* encodedVertexFeatureIds,
	std::vector<float>* encodedInstanceFeatureIds
) {
	CesiumFeatureStyleEncoding result;
	result.requested = description.enabled;
	result.usesInstanceFeatures = description.useInstanceFeatures;
	if (!description.enabled) {
		return result;
	}
	const std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>& features =
		description.useInstanceFeatures ? instanceFeatures : meshFeatures;
	if (features == nullptr || features->featureIdSets.empty()) {
		result.diagnostic = description.useInstanceFeatures
			? "primitive has no EXT_instance_features"
			: "primitive has no EXT_mesh_features";
		return result;
	}

	int32_t selectedIndex = -1;
	int32_t featureTextureCounter = 0;
	for (int32_t index = 0;
		index < static_cast<int32_t>(features->featureIdSets.size());
		++index) {
		const auto& candidate = features->featureIdSets[static_cast<size_t>(index)];
		if (!candidate) {
			continue;
		}
		const std::string name = get_feature_id_set_name(
			*candidate,
			&featureTextureCounter
		);
		if (
			!description.featureIdSetName.empty() &&
			name == description.featureIdSetName
		) {
			selectedIndex = index;
			break;
		}
	}
	if (description.featureIdSetName.empty()) {
		selectedIndex = description.featureIdSetIndex;
	}
	if (
		selectedIndex < 0 ||
		selectedIndex >= static_cast<int32_t>(features->featureIdSets.size())
	) {
		result.diagnostic = description.featureIdSetName.empty()
			? "feature ID set index is not present on this primitive"
			: "feature ID set name is not present on this primitive";
		return result;
	}
	const std::shared_ptr<CesiumFeatureIdSetSnapshot>& set =
		features->featureIdSets[static_cast<size_t>(selectedIndex)];
	if (
		set == nullptr || set->status != CesiumFeatureIdSetStatus::Valid ||
		set->featureCount <= 0
	) {
		result.diagnostic = "selected feature ID set is invalid";
		return result;
	}
	if (
		static_cast<uint64_t>(set->featureCount - 1) >
		static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())
	) {
		result.diagnostic = "selected feature ID set exceeds uint32 GPU IDs";
		return result;
	}

	result.featureIdSetIndex = selectedIndex;
	result.featureCount = set->featureCount;
	result.nullFeatureId = set->nullFeatureId;
	result.propertyTableIndex = set->propertyTableIndex;
	featureTextureCounter = 0;
	for (int32_t index = 0; index <= selectedIndex; ++index) {
		const auto& candidate = features->featureIdSets[static_cast<size_t>(index)];
		if (candidate) {
			result.name = get_feature_id_set_name(
				*candidate,
				&featureTextureCounter
			);
		}
	}
	result.featureIdSet = set;

	if (description.useInstanceFeatures) {
		if (
			set->type != CesiumFeatureIdSetType::Instance &&
			set->type != CesiumFeatureIdSetType::InstanceImplicit
		) {
			result.diagnostic = "selected set is not an instance feature ID set";
			return result;
		}
		if (encodedInstanceFeatureIds == nullptr || features->instanceCount <= 0) {
			result.diagnostic = "primitive has no GPU instances to style";
			return result;
		}
		encodedInstanceFeatureIds->resize(
			static_cast<size_t>(features->instanceCount) * 4
		);
		for (int64_t index = 0; index < features->instanceCount; ++index) {
			const int64_t id = set->type == CesiumFeatureIdSetType::InstanceImplicit
				? index
				: set->instanceFeatureIds[static_cast<size_t>(index)];
			const bool valid =
				(id >= 0 && id < set->featureCount) || id == set->nullFeatureId;
			const Vector3 encoded = encode_feature_id(id, valid);
			const size_t offset = static_cast<size_t>(index) * 4;
			(*encodedInstanceFeatureIds)[offset] = encoded.x;
			(*encodedInstanceFeatureIds)[offset + 1] = encoded.y;
			(*encodedInstanceFeatureIds)[offset + 2] = encoded.z;
			(*encodedInstanceFeatureIds)[offset + 3] = 0.0f;
		}
		result.source = CesiumFeatureStyleSource::Instance;
		result.valid = true;
		result.diagnostic = "encoded instance feature IDs in INSTANCE_CUSTOM";
		return result;
	}

	if (set->type == CesiumFeatureIdSetType::Texture) {
		if (
			set->textureCoordinateSetIndex < 0 ||
			set->textureCoordinateSetIndex > 1 || !set->textureView ||
			set->textureChannels.empty()
		) {
			result.diagnostic =
				"feature ID texture needs an unrealized texture-coordinate set";
			return result;
		}
		result.source = CesiumFeatureStyleSource::Texture;
		result.valid = true;
		result.diagnostic = "encoded feature ID texture for nearest GPU sampling";
		return result;
	}
	if (
		set->type != CesiumFeatureIdSetType::Attribute &&
		set->type != CesiumFeatureIdSetType::Implicit
	) {
		result.diagnostic = "selected set is not a mesh feature ID set";
		return result;
	}
	if (encodedVertexFeatureIds == nullptr || features->vertexCount <= 0) {
		result.diagnostic = "primitive has no vertices to style";
		return result;
	}
	if (
		set->type == CesiumFeatureIdSetType::Attribute &&
		set->vertexFeatureIds.size() !=
			static_cast<size_t>(features->vertexCount)
	) {
		result.diagnostic =
			"feature ID attribute count does not match the primitive vertex count";
		return result;
	}
	encodedVertexFeatureIds->resize(static_cast<int32_t>(features->vertexCount));
	for (int64_t index = 0; index < features->vertexCount; ++index) {
		const int64_t id = set->type == CesiumFeatureIdSetType::Implicit
			? (index < set->featureCount ? index : -1)
			: set->vertexFeatureIds[static_cast<size_t>(index)];
		const bool valid =
			(id >= 0 && id < set->featureCount) || id == set->nullFeatureId;
		encodedVertexFeatureIds->set(
			static_cast<int32_t>(index),
			encode_feature_id(id, valid)
		);
	}
	result.source = CesiumFeatureStyleSource::Vertex;
	result.valid = true;
	result.diagnostic = "encoded mesh feature IDs in CUSTOM0";
	return result;
}

int32_t get_effective_texture_coordinate_set(
	const CesiumGltf::TextureInfo& textureInfo
) {
	const auto* transform = textureInfo.getExtension<
		CesiumGltf::ExtensionKhrTextureTransform
	>();
	if (transform != nullptr && transform->texCoord) {
		return static_cast<int32_t>(*transform->texCoord);
	}
	return textureInfo.texCoord;
}

Error validate_texture_info_coordinates(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::TextureInfo* textureInfo
) {
	if (textureInfo == nullptr) {
		return Error::OK;
	}
	const int32_t coordinateSet =
		get_effective_texture_coordinate_set(*textureInfo);
	if (coordinateSet < 0 || coordinateSet > 1) {
		return Error::ERR_UNAVAILABLE;
	}
	const std::string semantic =
		"TEXCOORD_" + std::to_string(coordinateSet);
	return primitive.attributes.find(semantic) != primitive.attributes.end()
		? Error::OK
		: Error::ERR_INVALID_DATA;
}

Error validate_material_texture_coordinates(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Material* material
) {
	if (material == nullptr) {
		return Error::OK;
	}
	if (material->pbrMetallicRoughness) {
		const auto& pbr = *material->pbrMetallicRoughness;
		Error error = validate_texture_info_coordinates(
			primitive,
			pbr.baseColorTexture ? &*pbr.baseColorTexture : nullptr
		);
		if (error != Error::OK) {
			return error;
		}
		error = validate_texture_info_coordinates(
			primitive,
			pbr.metallicRoughnessTexture
				? &*pbr.metallicRoughnessTexture
				: nullptr
		);
		if (error != Error::OK) {
			return error;
		}
	}
	Error error = validate_texture_info_coordinates(
		primitive,
		material->normalTexture ? &*material->normalTexture : nullptr
	);
	if (error != Error::OK) {
		return error;
	}
	error = validate_texture_info_coordinates(
		primitive,
		material->occlusionTexture ? &*material->occlusionTexture : nullptr
	);
	if (error != Error::OK) {
		return error;
	}
	return validate_texture_info_coordinates(
		primitive,
		material->emissiveTexture ? &*material->emissiveTexture : nullptr
	);
}

template <typename TextureInfoType>
uint64_t count_new_texture_bytes(
	const CesiumGltf::Model& model,
	const TextureInfoType* textureInfo,
	std::unordered_set<const CesiumImage::ImageAsset*>* countedImages
) {
	if (textureInfo == nullptr || countedImages == nullptr) {
		return 0;
	}
	const CesiumGltf::Texture* texture = CesiumGltf::Model::getSafe(
		&model.textures,
		textureInfo->index
	);
	if (texture == nullptr) {
		return 0;
	}
	const CesiumGltf::Image* image = CesiumGltf::Model::getSafe(
		&model.images,
		texture->source
	);
	if (
		image == nullptr ||
		image->pAsset == nullptr ||
		!countedImages->insert(image->pAsset.get()).second
	) {
		return 0;
	}
	return static_cast<uint64_t>(
		std::max<int64_t>(0, image->pAsset->getSizeBytes())
	);
}

uint64_t count_new_material_texture_bytes(
	const CesiumGltf::Model& model,
	const CesiumGltf::Material* material,
	std::unordered_set<const CesiumImage::ImageAsset*>* countedImages
) {
	if (material == nullptr) {
		return 0;
	}
	uint64_t result = 0;
	if (material->pbrMetallicRoughness) {
		const auto& pbr = *material->pbrMetallicRoughness;
		result += count_new_texture_bytes(
			model,
			pbr.baseColorTexture ? &*pbr.baseColorTexture : nullptr,
			countedImages
		);
		result += count_new_texture_bytes(
			model,
			pbr.metallicRoughnessTexture
				? &*pbr.metallicRoughnessTexture
				: nullptr,
			countedImages
		);
	}
	result += count_new_texture_bytes(
		model,
		material->normalTexture ? &*material->normalTexture : nullptr,
		countedImages
	);
	result += count_new_texture_bytes(
		model,
		material->occlusionTexture ? &*material->occlusionTexture : nullptr,
		countedImages
	);
	result += count_new_texture_bytes(
		model,
		material->emissiveTexture ? &*material->emissiveTexture : nullptr,
		countedImages
	);
	return result;
}

Vector<Vector2> get_normal_map_coordinates(
	const CesiumGltf::Material* material,
	const Vector<Vector2>& uv0,
	const Vector<Vector2>& uv1,
	Error* error
) {
	Vector<Vector2> result;
	if (material == nullptr || !material->normalTexture) {
		return result;
	}
	const CesiumGltf::TextureInfo& textureInfo = *material->normalTexture;
	const int32_t coordinateSet =
		get_effective_texture_coordinate_set(textureInfo);
	result = coordinateSet == 1 ? uv1 : uv0;
	const auto* extension = textureInfo.getExtension<
		CesiumGltf::ExtensionKhrTextureTransform
	>();
	if (extension == nullptr) {
		return result;
	}
	CesiumGltf::KhrTextureTransform transform(*extension);
	if (transform.status() != CesiumGltf::KhrTextureTransformStatus::Valid) {
		*error = Error::ERR_INVALID_DATA;
		return Vector<Vector2>();
	}
	for (int32_t i = 0; i < result.size(); ++i) {
		const glm::dvec2 transformed = transform.applyTransform(
			result[i].x,
			result[i].y
		);
		result.set(i, Vector2(transformed.x, transformed.y));
	}
	return result;
}

struct MikkTangentData {
	const Vector<Vector3>* vertices = nullptr;
	const Vector<Vector3>* normals = nullptr;
	const Vector<Vector2>* textureCoordinates = nullptr;
	Vector<Vector4>* tangents = nullptr;
};

int mikk_get_num_faces(const SMikkTSpaceContext* context) {
	const auto* data = static_cast<const MikkTangentData*>(context->m_pUserData);
	return data->vertices->size() / 3;
}

int mikk_get_num_vertices_of_face(
	const SMikkTSpaceContext* context,
	int faceIndex
) {
	return faceIndex >= 0 && faceIndex < mikk_get_num_faces(context) ? 3 : 0;
}

void mikk_get_position(
	const SMikkTSpaceContext* context,
	float output[3],
	int faceIndex,
	int vertexIndex
) {
	const auto* data = static_cast<const MikkTangentData*>(context->m_pUserData);
	const Vector3& value = (*data->vertices)[faceIndex * 3 + vertexIndex];
	output[0] = value.x;
	output[1] = value.y;
	output[2] = value.z;
}

void mikk_get_normal(
	const SMikkTSpaceContext* context,
	float output[3],
	int faceIndex,
	int vertexIndex
) {
	const auto* data = static_cast<const MikkTangentData*>(context->m_pUserData);
	const Vector3& value = (*data->normals)[faceIndex * 3 + vertexIndex];
	output[0] = value.x;
	output[1] = value.y;
	output[2] = value.z;
}

void mikk_get_texture_coordinate(
	const SMikkTSpaceContext* context,
	float output[2],
	int faceIndex,
	int vertexIndex
) {
	const auto* data = static_cast<const MikkTangentData*>(context->m_pUserData);
	const Vector2& value =
		(*data->textureCoordinates)[faceIndex * 3 + vertexIndex];
	output[0] = value.x;
	output[1] = value.y;
}

void mikk_set_tangent(
	const SMikkTSpaceContext* context,
	const float tangent[3],
	float bitangentSign,
	int faceIndex,
	int vertexIndex
) {
	auto* data = static_cast<MikkTangentData*>(context->m_pUserData);
	data->tangents->set(
		faceIndex * 3 + vertexIndex,
		Vector4(tangent[0], tangent[1], tangent[2], bitangentSign)
	);
}

Error generate_tangent_space(
	const Vector<Vector3>& vertices,
	const Vector<int32_t>& indices,
	const Vector<Vector3>& normals,
	const Vector<Vector2>& textureCoordinates,
	Vector<Vector4>* tangents
) {
	if (
		tangents == nullptr ||
		vertices.size() != normals.size() ||
		vertices.size() != textureCoordinates.size() ||
		indices.size() != vertices.size() ||
		indices.size() % 3 != 0
	) {
		return Error::ERR_INVALID_DATA;
	}
	for (int32_t index = 0; index < indices.size(); ++index) {
		if (indices[index] != index) {
			// MikkTSpace returns unindexed tangent corners. Callers must expand
			// shared vertices first rather than overwriting a welded tangent.
			return Error::ERR_INVALID_DATA;
		}
	}

	tangents->resize(vertices.size());
	MikkTangentData data{
		&vertices,
		&normals,
		&textureCoordinates,
		tangents
	};
	SMikkTSpaceInterface interface{};
	interface.m_getNumFaces = mikk_get_num_faces;
	interface.m_getNumVerticesOfFace = mikk_get_num_vertices_of_face;
	interface.m_getPosition = mikk_get_position;
	interface.m_getNormal = mikk_get_normal;
	interface.m_getTexCoord = mikk_get_texture_coordinate;
	interface.m_setTSpaceBasic = mikk_set_tangent;

	SMikkTSpaceContext context{};
	context.m_pInterface = &interface;
	context.m_pUserData = &data;
	return genTangSpaceDefault(&context) != 0
		? Error::OK
		: Error::ERR_INVALID_DATA;
}

Error apply_node_transform(
	const glm::dmat4& transform,
	int32_t primitiveMode,
	Vector<Vector3>* vertices,
	Vector<Vector3>* normals,
	Vector<Vector4>* tangents,
	Vector<int32_t>* indices
) {
	if (vertices == nullptr || normals == nullptr || tangents == nullptr) {
		return Error::ERR_INVALID_PARAMETER;
	}
	const glm::dmat3 linear(transform);
	const double determinant = glm::determinant(linear);
	if (
		Math::abs(determinant) <= 1e-18 &&
		(!normals->is_empty() || !tangents->is_empty())
	) {
		return Error::ERR_INVALID_DATA;
	}
	for (int32_t i = 0; i < vertices->size(); ++i) {
		const Vector3 value = (*vertices)[i];
		const glm::dvec4 transformed = transform * glm::dvec4(
			value.x,
			value.y,
			value.z,
			1.0
		);
		if (Math::abs(transformed.w) <= 1e-18) {
			return Error::ERR_INVALID_DATA;
		}
		vertices->set(
			i,
			Vector3(
				transformed.x / transformed.w,
				transformed.y / transformed.w,
				transformed.z / transformed.w
			)
		);
	}
	if (!normals->is_empty()) {
		const glm::dmat3 normalMatrix = glm::transpose(glm::inverse(linear));
		for (int32_t i = 0; i < normals->size(); ++i) {
			const Vector3 value = (*normals)[i];
			const glm::dvec3 transformed = normalMatrix * glm::dvec3(
				value.x,
				value.y,
				value.z
			);
			normals->set(
				i,
				Vector3(transformed.x, transformed.y, transformed.z).normalized()
			);
		}
	}
	const double handedness = determinant < 0.0 ? -1.0 : 1.0;
	for (int32_t i = 0; i < tangents->size(); ++i) {
		const Vector4 value = (*tangents)[i];
		const glm::dvec3 transformed = linear * glm::dvec3(
			value.x,
			value.y,
			value.z
		);
		const Vector3 tangent = Vector3(
			transformed.x,
			transformed.y,
			transformed.z
		).normalized();
		tangents->set(
			i,
			Vector4(tangent.x, tangent.y, tangent.z, value.w * handedness)
		);
	}
	if (
		determinant < 0.0 &&
		indices != nullptr &&
		is_triangle_mode(primitiveMode)
	) {
		for (int32_t i = 0; i + 2 < indices->size(); i += 3) {
			const int32_t temporary = (*indices)[i + 1];
			indices->set(i + 1, (*indices)[i + 2]);
			indices->set(i + 2, temporary);
		}
	}
	return Error::OK;
}

const CesiumGltf::Accessor* get_instance_accessor(
	const CesiumGltf::Model& model,
	const CesiumGltf::ExtensionExtMeshGpuInstancing& extension,
	const char* semantic
) {
	const auto attribute = extension.attributes.find(semantic);
	return attribute == extension.attributes.end()
		? nullptr
		: CesiumGltf::Model::getSafe(&model.accessors, attribute->second);
}

template <typename T>
double normalized_rotation_component(T value) {
	if constexpr (std::is_floating_point_v<T>) {
		return static_cast<double>(value);
	} else {
		return std::max(
			-1.0,
			static_cast<double>(value) /
				static_cast<double>(std::numeric_limits<T>::max())
		);
	}
}

template <typename T>
Error apply_instance_rotations(
	const CesiumGltf::Model& model,
	const CesiumGltf::Accessor& accessor,
	std::vector<glm::dmat4>* transforms
) {
	CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC4<T>> view(
		model,
		accessor
	);
	if (
		transforms == nullptr ||
		view.status() != CesiumGltf::AccessorViewStatus::Valid ||
		view.size() != static_cast<int64_t>(transforms->size())
	) {
		return Error::ERR_INVALID_DATA;
	}
	for (int64_t index = 0; index < view.size(); ++index) {
		const auto& value = view[index].value;
		const glm::dquat rotation(
			normalized_rotation_component(value[3]),
			normalized_rotation_component(value[0]),
			normalized_rotation_component(value[1]),
			normalized_rotation_component(value[2])
		);
		if (
			!std::isfinite(rotation.x) ||
			!std::isfinite(rotation.y) ||
			!std::isfinite(rotation.z) ||
			!std::isfinite(rotation.w)
		) {
			return Error::ERR_INVALID_DATA;
		}
		const double length = glm::length(rotation);
		if (!std::isfinite(length) || length <= 1e-18) {
			return Error::ERR_INVALID_DATA;
		}
		(*transforms)[static_cast<size_t>(index)] *=
			glm::mat4_cast(rotation / length);
	}
	return Error::OK;
}

Error prepare_gpu_instance_transforms(
	const CesiumGltf::Model& model,
	const CesiumGltf::ExtensionExtMeshGpuInstancing& extension,
	const glm::dmat4& nodeTransform,
	const Vector<Vector3>& vertices,
	std::vector<float>* output,
	int32_t* outputCount,
	AABB* outputBounds,
	bool* outputRequiresUnculledMaterial
) {
	if (
		output == nullptr ||
		outputCount == nullptr ||
		outputBounds == nullptr ||
		outputRequiresUnculledMaterial == nullptr ||
		vertices.is_empty()
	) {
		return Error::ERR_INVALID_PARAMETER;
	}
	const CesiumGltf::Accessor* translation =
		get_instance_accessor(model, extension, "TRANSLATION");
	const CesiumGltf::Accessor* rotation =
		get_instance_accessor(model, extension, "ROTATION");
	const CesiumGltf::Accessor* scale =
		get_instance_accessor(model, extension, "SCALE");
	for (const auto& semanticAndAccessor : {
		std::pair<const char*, const CesiumGltf::Accessor*>(
			"TRANSLATION",
			translation
		),
		std::pair<const char*, const CesiumGltf::Accessor*>("ROTATION", rotation),
		std::pair<const char*, const CesiumGltf::Accessor*>("SCALE", scale)
	}) {
		if (
			extension.attributes.find(semanticAndAccessor.first) !=
				extension.attributes.end() &&
			semanticAndAccessor.second == nullptr
		) {
			return Error::ERR_INVALID_DATA;
		}
	}
	if (translation == nullptr && rotation == nullptr && scale == nullptr) {
		return Error::ERR_INVALID_DATA;
	}

	int64_t count = -1;
	for (const CesiumGltf::Accessor* accessor : {translation, rotation, scale}) {
		if (accessor == nullptr) {
			continue;
		}
		if (
			accessor->count <= 0 ||
			(count >= 0 && count != accessor->count)
		) {
			return Error::ERR_INVALID_DATA;
		}
		count = accessor->count;
	}
	if (
		count <= 0 ||
		count > std::numeric_limits<int32_t>::max() / 12
	) {
		return Error::ERR_OUT_OF_MEMORY;
	}

	std::vector<glm::dmat4> transforms(
		static_cast<size_t>(count),
		glm::dmat4(1.0)
	);
	if (translation != nullptr) {
		if (
			translation->type != CesiumGltf::Accessor::Type::VEC3 ||
			translation->componentType !=
				CesiumGltf::Accessor::ComponentType::FLOAT
		) {
			return Error::ERR_INVALID_DATA;
		}
		CesiumGltf::AccessorView<glm::fvec3> view(model, *translation);
		if (
			view.status() != CesiumGltf::AccessorViewStatus::Valid ||
			view.size() != count
		) {
			return Error::ERR_INVALID_DATA;
		}
		for (int64_t index = 0; index < count; ++index) {
			transforms[static_cast<size_t>(index)] = glm::translate(
				transforms[static_cast<size_t>(index)],
				glm::dvec3(view[index])
			);
		}
	}
	if (rotation != nullptr) {
		if (
			rotation->type != CesiumGltf::Accessor::Type::VEC4 ||
			(
				rotation->componentType ==
					CesiumGltf::Accessor::ComponentType::FLOAT &&
				rotation->normalized
			)
		) {
			return Error::ERR_INVALID_DATA;
		}
		Error rotationError = Error::ERR_INVALID_DATA;
		switch (rotation->componentType) {
		case CesiumGltf::Accessor::ComponentType::FLOAT:
			rotationError = apply_instance_rotations<float>(
				model,
				*rotation,
				&transforms
			);
			break;
		case CesiumGltf::Accessor::ComponentType::BYTE:
			if (rotation->normalized) {
				rotationError = apply_instance_rotations<int8_t>(
					model,
					*rotation,
					&transforms
				);
			}
			break;
		case CesiumGltf::Accessor::ComponentType::SHORT:
			if (rotation->normalized) {
				rotationError = apply_instance_rotations<int16_t>(
					model,
					*rotation,
					&transforms
				);
			}
			break;
		default:
			break;
		}
		if (rotationError != Error::OK) {
			return rotationError;
		}
	}
	if (scale != nullptr) {
		if (
			scale->type != CesiumGltf::Accessor::Type::VEC3 ||
			scale->componentType != CesiumGltf::Accessor::ComponentType::FLOAT
		) {
			return Error::ERR_INVALID_DATA;
		}
		CesiumGltf::AccessorView<glm::fvec3> view(model, *scale);
		if (
			view.status() != CesiumGltf::AccessorViewStatus::Valid ||
			view.size() != count
		) {
			return Error::ERR_INVALID_DATA;
		}
		for (int64_t index = 0; index < count; ++index) {
			transforms[static_cast<size_t>(index)] = glm::scale(
				transforms[static_cast<size_t>(index)],
				glm::dvec3(view[index])
			);
		}
	}

	glm::dvec3 sourceMinimum(std::numeric_limits<double>::max());
	glm::dvec3 sourceMaximum(std::numeric_limits<double>::lowest());
	for (const Vector3& vertex : vertices) {
		const glm::dvec3 value(vertex.x, vertex.y, vertex.z);
		sourceMinimum = glm::min(sourceMinimum, value);
		sourceMaximum = glm::max(sourceMaximum, value);
	}
	glm::dvec3 instanceMinimum(std::numeric_limits<double>::max());
	glm::dvec3 instanceMaximum(std::numeric_limits<double>::lowest());
	bool requiresUnculledMaterial = false;
	output->resize(static_cast<size_t>(count * 12));
	for (int64_t index = 0; index < count; ++index) {
		const glm::dmat4 transform = transforms[static_cast<size_t>(index)];
		const glm::dmat4 combinedTransform = nodeTransform * transform;
		requiresUnculledMaterial = requiresUnculledMaterial ||
			glm::determinant(glm::dmat3(combinedTransform)) < 0.0;
		const int32_t offset = static_cast<int32_t>(index * 12);
		for (int32_t row = 0; row < 3; ++row) {
			for (int32_t column = 0; column < 4; ++column) {
				const double value = transform[column][row];
				if (!std::isfinite(value)) {
					return Error::ERR_INVALID_DATA;
				}
				(*output)[static_cast<size_t>(offset + row * 4 + column)] =
					static_cast<float>(value);
			}
		}
		for (int32_t corner = 0; corner < 8; ++corner) {
			const glm::dvec3 sourceCorner(
				(corner & 1) != 0 ? sourceMaximum.x : sourceMinimum.x,
				(corner & 2) != 0 ? sourceMaximum.y : sourceMinimum.y,
				(corner & 4) != 0 ? sourceMaximum.z : sourceMinimum.z
			);
			const glm::dvec4 transformed =
				transform * glm::dvec4(sourceCorner, 1.0);
			if (
				!std::isfinite(transformed.x) ||
				!std::isfinite(transformed.y) ||
				!std::isfinite(transformed.z) ||
				!std::isfinite(transformed.w) ||
				Math::abs(transformed.w) <= 1e-18
			) {
				return Error::ERR_INVALID_DATA;
			}
			const glm::dvec3 point = glm::dvec3(transformed) / transformed.w;
			instanceMinimum = glm::min(instanceMinimum, point);
			instanceMaximum = glm::max(instanceMaximum, point);
		}
	}
	*outputCount = static_cast<int32_t>(count);
	*outputRequiresUnculledMaterial = requiresUnculledMaterial;
	*outputBounds = AABB(
		Vector3(
			instanceMinimum.x,
			instanceMinimum.y,
			instanceMinimum.z
		),
		Vector3(
			instanceMaximum.x - instanceMinimum.x,
			instanceMaximum.y - instanceMinimum.y,
			instanceMaximum.z - instanceMinimum.z
		)
	);
	return Error::OK;
}
}

std::vector<CesiumGDPrimitiveInstance>
CesiumGDModelLoader::collect_primitive_instances(
	const CesiumGltf::Model& model
) {
	std::vector<CesiumGDPrimitiveInstance> result;
	glm::dmat4 modelRoot(1.0);
	modelRoot = apply_rtc_center(model, modelRoot);
	modelRoot = apply_gltf_up_axis_transform(model, modelRoot);
	model.forEachPrimitiveInScene(
		-1,
		[&result, &modelRoot](
			const CesiumGltf::Model& traversedModel,
			const CesiumGltf::Node& node,
			const CesiumGltf::Mesh& mesh,
			const CesiumGltf::MeshPrimitive& primitive,
			const glm::dmat4& transform
		) {
			CesiumGDPrimitiveInstance instance;
			for (size_t index = 0; index < traversedModel.nodes.size(); ++index) {
				if (&node == &traversedModel.nodes[index]) {
					instance.nodeIndex = static_cast<int32_t>(index);
					break;
				}
			}
			for (size_t index = 0; index < traversedModel.meshes.size(); ++index) {
				if (&mesh == &traversedModel.meshes[index]) {
					instance.meshIndex = static_cast<int32_t>(index);
					break;
				}
			}
			for (size_t index = 0; index < mesh.primitives.size(); ++index) {
				if (&primitive == &mesh.primitives[index]) {
					instance.primitiveIndex = static_cast<int32_t>(index);
					break;
				}
			}
			instance.transform = modelRoot * transform;
			result.push_back(instance);
		}
	);
	return result;
}

CesiumGDPreparedModel* CesiumGDModelLoader::prepare_model(
	const CesiumGltf::Model& model,
	Error* error,
	const CesiumFeatureStyleEncodingDescription& featureStyle
) {
	ERR_FAIL_NULL_V(error, nullptr);
	*error = Error::OK;
	std::unique_ptr<CesiumGDPreparedModel> prepared =
		std::make_unique<CesiumGDPreparedModel>();
	prepared->metadataSnapshot = create_cesium_metadata_snapshot(model);
	if (prepared->metadataSnapshot != nullptr) {
		prepared->cpuImageAssets = prepared->metadataSnapshot->cpuImageAssets;
	}
	for (const CesiumGltf::Image& image : model.images) {
		if (image.pAsset != nullptr) {
			prepared->imageContentFingerprints.try_emplace(
				image.pAsset.get(),
				CesiumGltfImageAssetResourceCache::fingerprint(*image.pAsset)
			);
		}
	}

	const std::vector<CesiumGDPrimitiveInstance> primitiveInstances =
		collect_primitive_instances(model);
	prepared->primitives.reserve(primitiveInstances.size());
	std::unordered_set<const CesiumImage::ImageAsset*> countedImages;
	std::unordered_map<
		int32_t,
		std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
	> instanceFeaturesByNode;
	std::unordered_set<const CesiumPrimitiveFeaturesSnapshot*>
		countedInstanceFeatureSnapshots;
	for (const CesiumGDPrimitiveInstance& primitiveInstance : primitiveInstances) {
		if (
			primitiveInstance.nodeIndex < 0 ||
			primitiveInstance.nodeIndex >= static_cast<int32_t>(model.nodes.size()) ||
			primitiveInstance.meshIndex < 0 ||
			primitiveInstance.meshIndex >= static_cast<int32_t>(model.meshes.size())
		) {
			*error = Error::ERR_INVALID_DATA;
			return nullptr;
		}
		const CesiumGltf::Mesh& mesh = model.meshes[primitiveInstance.meshIndex];
		if (
			primitiveInstance.primitiveIndex < 0 ||
			primitiveInstance.primitiveIndex >=
				static_cast<int32_t>(mesh.primitives.size())
		) {
			*error = Error::ERR_INVALID_DATA;
			return nullptr;
		}
		const CesiumGltf::MeshPrimitive& primitive =
			mesh.primitives[primitiveInstance.primitiveIndex];
		const CesiumGltf::Material* gltfMaterial =
			CesiumGltf::Model::getSafe(
				&model.materials,
				primitive.material
			);
		*error = validate_material_texture_coordinates(
			primitive,
			gltfMaterial
		);
		if (*error != Error::OK) {
			ERR_PRINT(
				"glTF material references a texture-coordinate set that "
				"the primitive does not provide or Godot cannot realize"
			);
			return nullptr;
		}

		Vector<Vector3> vertices = get_positions(primitive, model, error);
			if (*error != Error::OK || vertices.is_empty()) {
				if (*error == Error::OK) {
					*error = Error::ERR_INVALID_DATA;
				}
				ERR_PRINT("glTF primitive has no valid POSITION accessor");
				return nullptr;
			}

			Vector<int32_t> indices = get_index_buffer_from_primitive(
				primitive,
				model,
				vertices.size(),
				error
			);
			if (*error != Error::OK) {
				return nullptr;
			}
			Vector<Vector3> normals = get_normals(primitive, model, error);
			Vector<Vector4> tangents = get_tangents(primitive, model, error);
			Vector<Color> colors = get_colors(primitive, model, error);
			Vector<Vector2> textureCoords = get_texture_coordinates(
				primitive,
				model,
				"TEXCOORD_0",
				error
			);
			Vector<Vector2> textureCoords2 = get_texture_coordinates(
				primitive,
				model,
				"TEXCOORD_1",
				error
			);
			if (*error != Error::OK) {
				return nullptr;
			}

			// Quantized-mesh terrain commonly supplies overlay coordinates instead
			// of ordinary glTF UVs. Preserve the established UV0/UV1 mapping while
			// making unsupported collisions explicit in the primitive context.
			if (textureCoords.is_empty()) {
				textureCoords = get_texture_coordinates(
					primitive,
					model,
					"_CESIUMOVERLAY_0",
					error
				);
			}
			if (textureCoords2.is_empty()) {
				textureCoords2 = get_texture_coordinates(
					primitive,
					model,
					"_CESIUMOVERLAY_1",
					error
				);
			}
			if (*error != Error::OK) {
				return nullptr;
			}

			const CesiumGltf::Node& sourceNode =
				model.nodes[primitiveInstance.nodeIndex];
			const auto* gpuInstancing = sourceNode.getExtension<
				CesiumGltf::ExtensionExtMeshGpuInstancing
			>();
			std::vector<float> instanceTransformBuffer;
			int32_t instanceCount = 0;
			AABB instanceBounds;
			bool requiresUnculledInstancing = false;
			if (gpuInstancing != nullptr) {
				*error = prepare_gpu_instance_transforms(
					model,
					*gpuInstancing,
					primitiveInstance.transform,
					vertices,
					&instanceTransformBuffer,
					&instanceCount,
					&instanceBounds,
					&requiresUnculledInstancing
				);
			} else {
				*error = apply_node_transform(
					primitiveInstance.transform,
					primitive.mode,
					&vertices,
					&normals,
					&tangents,
					&indices
				);
			}
			if (*error != Error::OK) {
				if (gpuInstancing != nullptr) {
					ERR_PRINT("glTF EXT_mesh_gpu_instancing data is invalid");
				}
				return nullptr;
			}

			const Vector<int32_t> attributeCounts = {
				static_cast<int32_t>(normals.size()),
				static_cast<int32_t>(tangents.size()),
				static_cast<int32_t>(colors.size()),
				static_cast<int32_t>(textureCoords.size()),
				static_cast<int32_t>(textureCoords2.size())
			};
			const char* semantics[] = {
				"NORMAL", "TANGENT", "COLOR_0", "UV0", "UV1"
			};
			for (int32_t i = 0; i < attributeCounts.size(); ++i) {
				*error = validate_attribute_count(
					vertices.size(),
					attributeCounts[i],
					semantics[i]
				);
				if (*error != Error::OK) {
					return nullptr;
				}
			}

			std::vector<int64_t> featureTriangleIndices;
			std::vector<glm::dvec3> featureVertexPositions;
			if (
				primitive.getExtension<CesiumGltf::ExtensionExtMeshFeatures>() !=
					nullptr ||
				primitive.getExtension<
					CesiumGltf::ExtensionMeshPrimitiveExtStructuralMetadata
				>() != nullptr
			) {
				featureVertexPositions.reserve(
					static_cast<size_t>(vertices.size())
				);
				for (const Vector3& vertex : vertices) {
					featureVertexPositions.emplace_back(
						vertex.x,
						vertex.y,
						vertex.z
					);
				}
				if (is_triangle_mode(primitive.mode)) {
					featureTriangleIndices.reserve(
						static_cast<size_t>(indices.size())
					);
					for (int32_t index : indices) {
						featureTriangleIndices.emplace_back(index);
					}
				}
			}
			std::shared_ptr<CesiumPrimitiveFeaturesSnapshot> featuresSnapshot =
				create_cesium_primitive_features_snapshot(
					model,
					primitive,
					featureVertexPositions,
					featureTriangleIndices
				);
			if (featuresSnapshot != nullptr) {
				for (const auto& featureIdSet : featuresSnapshot->featureIdSets) {
					if (
						featureIdSet != nullptr && featureIdSet->textureView != nullptr &&
						featureIdSet->textureView->getImage() != nullptr
					) {
						prepared->cpuImageAssets.emplace(
							featureIdSet->textureView->getImage()
						);
					}
				}
			}
			std::shared_ptr<CesiumPrimitiveMetadataSnapshot> metadataSnapshot =
				create_cesium_primitive_metadata_snapshot(
					model,
					primitive,
					prepared->metadataSnapshot,
					featureVertexPositions,
					featureTriangleIndices
				);
			std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>
				instanceFeaturesSnapshot;
			if (gpuInstancing != nullptr) {
				const auto existing = instanceFeaturesByNode.find(
					primitiveInstance.nodeIndex
				);
				if (existing != instanceFeaturesByNode.end()) {
					instanceFeaturesSnapshot = existing->second;
				} else {
					instanceFeaturesSnapshot =
						create_cesium_instance_features_snapshot(
							model,
							sourceNode,
							instanceCount
						);
					instanceFeaturesByNode.emplace(
						primitiveInstance.nodeIndex,
						instanceFeaturesSnapshot
					);
				}
			}
			const bool countInstanceFeatures = instanceFeaturesSnapshot &&
				countedInstanceFeatureSnapshots.emplace(
					instanceFeaturesSnapshot.get()
				).second;
			Vector<Vector3> encodedFeatureIds;
			std::vector<float> encodedInstanceFeatureIds;
			CesiumFeatureStyleEncoding featureStyleEncoding =
				prepare_feature_style_encoding(
					featureStyle,
					featuresSnapshot,
					instanceFeaturesSnapshot,
					&encodedFeatureIds,
					&encodedInstanceFeatureIds
				);

			const bool unlit = gltfMaterial != nullptr &&
				gltfMaterial->hasExtension<
					CesiumGltf::ExtensionKhrMaterialsUnlit
				>();
			const bool needsFlatNormals =
				normals.is_empty() && is_triangle_mode(primitive.mode) && !unlit;
			const bool needsTangents =
				gltfMaterial != nullptr &&
				gltfMaterial->normalTexture.has_value() &&
				is_triangle_mode(primitive.mode) &&
				!unlit;
			const bool normalUsesDerivedTangentSpace =
				needsTangents && (
					get_effective_texture_coordinate_set(
						*gltfMaterial->normalTexture
					) != 0 ||
					gltfMaterial->normalTexture->hasExtension<
						CesiumGltf::ExtensionKhrTextureTransform
					>()
				);
			const bool needsGeneratedTangents =
				needsTangents && (
					tangents.is_empty() || normalUsesDerivedTangentSpace
				);
			if (needsFlatNormals || needsGeneratedTangents) {
				expand_triangle_vertices(
					&vertices,
					&normals,
					&tangents,
					&colors,
					&textureCoords,
					&textureCoords2,
					&encodedFeatureIds,
					&indices
				);
			}
			if (needsFlatNormals) {
				*error = generate_normals(&normals, vertices, indices);
				if (*error != Error::OK) {
					return nullptr;
				}
			}
			if (needsGeneratedTangents) {
				tangents.clear();
				Vector<Vector2> normalCoordinates = get_normal_map_coordinates(
					gltfMaterial,
					textureCoords,
					textureCoords2,
					error
				);
				if (*error == Error::OK) {
					*error = generate_tangent_space(
						vertices,
						indices,
						normals,
						normalCoordinates,
						&tangents
					);
				}
				if (*error != Error::OK) {
					return nullptr;
				}
			}

		CesiumGDPreparedPrimitive preparedPrimitive;
		preparedPrimitive.source = primitiveInstance;
		preparedPrimitive.isGpuInstanced = gpuInstancing != nullptr;
		preparedPrimitive.isTranslucent =
			gltfMaterial != nullptr &&
			gltfMaterial->alphaMode == CesiumGltf::Material::AlphaMode::BLEND;
		preparedPrimitive.requiresUnculledInstancing =
			requiresUnculledInstancing;
		preparedPrimitive.instanceCount = instanceCount;
		preparedPrimitive.instanceTransformBuffer =
			std::move(instanceTransformBuffer);
		preparedPrimitive.instanceBounds = instanceBounds;
		preparedPrimitive.vertices = std::move(vertices);
		preparedPrimitive.indices = std::move(indices);
		preparedPrimitive.normals = std::move(normals);
		preparedPrimitive.tangents = std::move(tangents);
		preparedPrimitive.colors = std::move(colors);
		preparedPrimitive.textureCoords = std::move(textureCoords);
		preparedPrimitive.textureCoords2 = std::move(textureCoords2);
		preparedPrimitive.encodedFeatureIds = std::move(encodedFeatureIds);
		preparedPrimitive.dimensions = compute_dimensions(
			preparedPrimitive.vertices
		);
		if (primitive.mode == CesiumGltf::MeshPrimitive::Mode::POINTS) {
			preparedPrimitive.pointCount = preparedPrimitive.indices.size();
			preparedPrimitive.pointDiameter = get_bentley_point_diameter(
				gltfMaterial
			);
		}
		preparedPrimitive.encodedInstanceFeatureIds =
			std::move(encodedInstanceFeatureIds);
		preparedPrimitive.featureStyleEncoding =
			std::move(featureStyleEncoding);
		preparedPrimitive.featuresSnapshot = std::move(featuresSnapshot);
		preparedPrimitive.instanceFeaturesSnapshot =
			std::move(instanceFeaturesSnapshot);
		preparedPrimitive.metadataSnapshot = std::move(metadataSnapshot);
		preparedPrimitive.geometryBytes =
			static_cast<uint64_t>(preparedPrimitive.vertices.size()) *
				sizeof(Vector3) +
			static_cast<uint64_t>(preparedPrimitive.indices.size()) *
				sizeof(int32_t) +
			static_cast<uint64_t>(preparedPrimitive.normals.size()) *
				sizeof(Vector3) +
			static_cast<uint64_t>(preparedPrimitive.tangents.size()) *
				sizeof(Vector4) +
			static_cast<uint64_t>(preparedPrimitive.colors.size()) *
				sizeof(Color) +
			static_cast<uint64_t>(preparedPrimitive.textureCoords.size()) *
				sizeof(Vector2) +
			static_cast<uint64_t>(preparedPrimitive.textureCoords2.size()) *
				sizeof(Vector2) +
			static_cast<uint64_t>(preparedPrimitive.encodedFeatureIds.size()) *
				sizeof(Vector3) +
			static_cast<uint64_t>(
				preparedPrimitive.encodedInstanceFeatureIds.size()
			) * sizeof(float) +
			static_cast<uint64_t>(
				preparedPrimitive.instanceTransformBuffer.size()
			) * sizeof(float) +
			(preparedPrimitive.featuresSnapshot
				? preparedPrimitive.featuresSnapshot->memoryBytes
				: 0) +
			(countInstanceFeatures && preparedPrimitive.instanceFeaturesSnapshot
				? preparedPrimitive.instanceFeaturesSnapshot->memoryBytes
				: 0) +
			(preparedPrimitive.metadataSnapshot
				? preparedPrimitive.metadataSnapshot->memoryBytes
				: 0);
		preparedPrimitive.textureBytes = count_new_material_texture_bytes(
			model,
			gltfMaterial,
			&countedImages
		) + (preparedPrimitive.featuresSnapshot
			? preparedPrimitive.featuresSnapshot->textureBytes
			: 0);
		prepared->geometryBytes += preparedPrimitive.geometryBytes;
		prepared->textureBytes += preparedPrimitive.textureBytes;
		prepared->maximumPrimitiveGeometryBytes = std::max(
			prepared->maximumPrimitiveGeometryBytes,
			preparedPrimitive.geometryBytes
		);
		prepared->maximumPrimitiveTextureBytes = std::max(
			prepared->maximumPrimitiveTextureBytes,
			preparedPrimitive.textureBytes
		);
		prepared->primitives.emplace_back(std::move(preparedPrimitive));
	}
	if (prepared->metadataSnapshot) {
		const uint64_t metadataMemoryBytes =
			prepared->metadataSnapshot->memoryBytes;
		const uint64_t metadataTextureBytes =
			prepared->metadataSnapshot->textureBytes;
		prepared->geometryBytes += metadataMemoryBytes;
		prepared->textureBytes += metadataTextureBytes;
		if (!prepared->primitives.empty()) {
			CesiumGDPreparedPrimitive& accountingPrimitive =
				prepared->primitives.front();
			accountingPrimitive.geometryBytes += metadataMemoryBytes;
			accountingPrimitive.textureBytes += metadataTextureBytes;
			prepared->maximumPrimitiveGeometryBytes = std::max(
				prepared->maximumPrimitiveGeometryBytes,
				accountingPrimitive.geometryBytes
			);
			prepared->maximumPrimitiveTextureBytes = std::max(
				prepared->maximumPrimitiveTextureBytes,
				accountingPrimitive.textureBytes
			);
		}
	}

	return prepared.release();
}

Ref<ArrayMesh> CesiumGDModelLoader::realize_prepared_model(
	const CesiumGltf::Model& model,
	CesiumGDPreparedModel& prepared,
	Error* error
) {
	ERR_FAIL_NULL_V(error, Ref<ArrayMesh>());
	*error = Error::OK;
	while (!realize_prepared_model_incrementally(model, prepared, error)) {
		if (*error != Error::OK) {
			return Ref<ArrayMesh>();
		}
	}
	return prepared.realizedMesh;
}

bool CesiumGDModelLoader::realize_prepared_model_incrementally(
	const CesiumGltf::Model& model,
	CesiumGDPreparedModel& prepared,
	Error* error
) {
	ERR_FAIL_NULL_V(error, true);
	*error = Error::OK;
	prepared.realizationFailureStage =
		CesiumGDRealizationFailureStage::Renderer;
	if (prepared.realizedMesh.is_null()) {
		prepared.realizedMesh.instantiate();
		prepared.materialLoader = std::make_shared<CesiumGltfMaterialLoader>(
			model,
			prepared.sharedImageCache,
			&prepared.imageContentFingerprints,
			&prepared.cpuImageAssets,
			&prepared.sharedImageResources,
			prepared.enableLodTransitionDither,
			prepared.enableTranslucencyDepthPrepass
		);
	}
	if (prepared.nextPrimitiveToRealize >= prepared.primitives.size()) {
		if (!prepared.metadataRealizationComplete) {
			if (
				prepared.metadataSnapshot == nullptr ||
				(
					prepared.metadataSnapshot->propertyTables.empty() &&
					prepared.metadataSnapshot->propertyTextures.empty() &&
					prepared.metadataSnapshot->enums.empty()
				)
			) {
				prepared.metadataRealizationComplete = true;
			} else if (prepared.realizedMetadata.is_null()) {
				prepared.realizedMetadata.instantiate();
				return false;
			} else if (
				prepared.nextMetadataEnumToRealize <
				prepared.metadataSnapshot->enums.size()
			) {
				Ref<CesiumMetadataEnum> metadataEnum;
				metadataEnum.instantiate();
				metadataEnum->initialize(prepared.metadataSnapshot->enums[
					prepared.nextMetadataEnumToRealize
				]);
				prepared.realizedMetadata->add_enum(metadataEnum);
				++prepared.nextMetadataEnumToRealize;
				return false;
			} else if (
				prepared.nextMetadataTableToRealize <
				prepared.metadataSnapshot->propertyTables.size()
			) {
				const CesiumPropertyTableSnapshot& tableSnapshot =
					prepared.metadataSnapshot->propertyTables[
						prepared.nextMetadataTableToRealize
					];
				if (prepared.currentMetadataTable.is_null()) {
					prepared.currentMetadataTable.instantiate();
					prepared.currentMetadataTable->initialize(tableSnapshot);
					prepared.realizedMetadata->add_property_table(
						prepared.currentMetadataTable
					);
					prepared.nextMetadataPropertyToRealize = 0;
					return false;
				}
				if (
					prepared.nextMetadataPropertyToRealize <
					tableSnapshot.properties.size()
				) {
					Ref<CesiumMetadataProperty> property;
					property.instantiate();
					property->initialize(tableSnapshot.properties[
						prepared.nextMetadataPropertyToRealize
					]);
					prepared.currentMetadataTable->add_property(property);
					++prepared.nextMetadataPropertyToRealize;
					return false;
				}
				prepared.currentMetadataTable.unref();
				++prepared.nextMetadataTableToRealize;
				prepared.nextMetadataPropertyToRealize = 0;
				return false;
			} else if (
				prepared.nextMetadataTextureToRealize <
				prepared.metadataSnapshot->propertyTextures.size()
			) {
				const auto& textureSnapshot =
					prepared.metadataSnapshot->propertyTextures[
						prepared.nextMetadataTextureToRealize
					];
				if (prepared.currentMetadataTexture.is_null()) {
					prepared.currentMetadataTexture.instantiate();
					prepared.currentMetadataTexture->initialize(textureSnapshot);
					prepared.realizedMetadata->add_property_texture(
						prepared.currentMetadataTexture
					);
					prepared.nextMetadataTexturePropertyToRealize = 0;
					return false;
				}
				if (
					textureSnapshot != nullptr &&
					prepared.nextMetadataTexturePropertyToRealize <
						textureSnapshot->properties.size()
				) {
					Ref<CesiumPropertyTextureProperty> property;
					property.instantiate();
					property->initialize(textureSnapshot->properties[
						prepared.nextMetadataTexturePropertyToRealize
					]);
					prepared.currentMetadataTexture->add_property(property);
					++prepared.nextMetadataTexturePropertyToRealize;
					return false;
				}
				prepared.currentMetadataTexture.unref();
				++prepared.nextMetadataTextureToRealize;
				prepared.nextMetadataTexturePropertyToRealize = 0;
				return false;
			} else {
				prepared.realizedMetadata->set_property_texture_bytes(
					static_cast<int64_t>(
						prepared.metadataSnapshot->textureBytes
					)
				);
				prepared.realizedMetadata->set_retained_memory_bytes(
					static_cast<int64_t>(
						prepared.metadataSnapshot->memoryBytes
					)
				);
				prepared.metadataRealizationComplete = true;
			}
		}

		if (
			prepared.nextPrimitiveFeaturesToRealize <
			prepared.primitives.size()
		) {
			const std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>& snapshot =
				prepared.primitives[
					prepared.nextPrimitiveFeaturesToRealize
				].featuresSnapshot;
			if (prepared.currentPrimitiveFeatures.is_null()) {
				prepared.currentPrimitiveFeatures.instantiate();
				prepared.currentPrimitiveFeatures->initialize(snapshot);
				prepared.realizedPrimitiveFeatures.push_back(
					prepared.currentPrimitiveFeatures
				);
				prepared.nextFeatureIdSetToRealize = 0;
				return false;
			}
			if (
				snapshot != nullptr &&
				prepared.nextFeatureIdSetToRealize <
					snapshot->featureIdSets.size()
			) {
				Ref<CesiumFeatureIdSet> featureIdSet;
				featureIdSet.instantiate();
				featureIdSet->initialize(snapshot->featureIdSets[
					prepared.nextFeatureIdSetToRealize
				]);
				prepared.currentPrimitiveFeatures->add_feature_id_set(featureIdSet);
				++prepared.nextFeatureIdSetToRealize;
				return false;
			}
			prepared.currentPrimitiveFeatures.unref();
			++prepared.nextPrimitiveFeaturesToRealize;
			prepared.nextFeatureIdSetToRealize = 0;
			return false;
		}

		if (
			prepared.nextPrimitiveMetadataToRealize <
			prepared.primitives.size()
		) {
			CesiumGDPreparedPrimitive& metadataPrimitive = prepared.primitives[
				prepared.nextPrimitiveMetadataToRealize
			];
			const auto& snapshot = metadataPrimitive.metadataSnapshot;
			if (snapshot == nullptr) {
				++prepared.nextPrimitiveMetadataToRealize;
				return false;
			}
			if (prepared.currentPrimitiveMetadata.is_null()) {
				prepared.currentPrimitiveMetadata.instantiate();
				prepared.currentPrimitiveMetadata->initialize(snapshot);
				metadataPrimitive.realizedPrimitiveMetadata =
					prepared.currentPrimitiveMetadata;
				prepared.nextPropertyAttributeToRealize = 0;
				prepared.nextPropertyAttributePropertyToRealize = 0;
				return false;
			}
			if (
				prepared.nextPropertyAttributeToRealize <
				snapshot->propertyAttributes.size()
			) {
				const auto& attributeSnapshot = snapshot->propertyAttributes[
					prepared.nextPropertyAttributeToRealize
				];
				if (prepared.currentPropertyAttribute.is_null()) {
					prepared.currentPropertyAttribute.instantiate();
					prepared.currentPropertyAttribute->initialize(attributeSnapshot);
					prepared.currentPrimitiveMetadata->add_property_attribute(
						prepared.currentPropertyAttribute
					);
					prepared.nextPropertyAttributePropertyToRealize = 0;
					return false;
				}
				if (
					attributeSnapshot != nullptr &&
					prepared.nextPropertyAttributePropertyToRealize <
						attributeSnapshot->properties.size()
				) {
					Ref<CesiumPropertyAttributeProperty> property;
					property.instantiate();
					property->initialize(attributeSnapshot->properties[
						prepared.nextPropertyAttributePropertyToRealize
					]);
					prepared.currentPropertyAttribute->add_property(property);
					++prepared.nextPropertyAttributePropertyToRealize;
					return false;
				}
				prepared.currentPropertyAttribute.unref();
				++prepared.nextPropertyAttributeToRealize;
				prepared.nextPropertyAttributePropertyToRealize = 0;
				return false;
			}
			prepared.currentPrimitiveMetadata.unref();
			++prepared.nextPrimitiveMetadataToRealize;
			prepared.nextPropertyAttributeToRealize = 0;
			prepared.nextPropertyAttributePropertyToRealize = 0;
			return false;
		}

		if (
			prepared.nextInstanceFeaturesToRealize >=
			prepared.primitives.size()
		) {
			return true;
		}
		CesiumGDPreparedPrimitive& instancePrimitive = prepared.primitives[
			prepared.nextInstanceFeaturesToRealize
		];
		const std::shared_ptr<CesiumPrimitiveFeaturesSnapshot>& snapshot =
			instancePrimitive.instanceFeaturesSnapshot;
		if (snapshot == nullptr) {
			++prepared.nextInstanceFeaturesToRealize;
			return false;
		}
		if (prepared.currentInstanceFeatures.is_null()) {
			const auto existing =
				prepared.realizedInstanceFeaturesBySnapshot.find(snapshot.get());
			if (
				existing !=
				prepared.realizedInstanceFeaturesBySnapshot.end()
			) {
				instancePrimitive.realizedInstanceFeatures = existing->second;
				++prepared.nextInstanceFeaturesToRealize;
				return false;
			}
			prepared.currentInstanceFeatures.instantiate();
			prepared.currentInstanceFeatures->initialize(snapshot);
			instancePrimitive.realizedInstanceFeatures =
				prepared.currentInstanceFeatures;
			prepared.realizedInstanceFeaturesBySnapshot.emplace(
				snapshot.get(),
				prepared.currentInstanceFeatures
			);
			prepared.nextInstanceFeatureIdSetToRealize = 0;
			return false;
		}
		if (
			prepared.nextInstanceFeatureIdSetToRealize <
			snapshot->featureIdSets.size()
		) {
			Ref<CesiumFeatureIdSet> featureIdSet;
			featureIdSet.instantiate();
			featureIdSet->initialize(snapshot->featureIdSets[
				prepared.nextInstanceFeatureIdSetToRealize
			]);
			prepared.currentInstanceFeatures->add_feature_id_set(featureIdSet);
			++prepared.nextInstanceFeatureIdSetToRealize;
			return false;
		}
		prepared.currentInstanceFeatures.unref();
		++prepared.nextInstanceFeaturesToRealize;
		prepared.nextInstanceFeatureIdSetToRealize = 0;
		return prepared.nextInstanceFeaturesToRealize >=
			prepared.primitives.size();
	}

	const size_t primitiveIndex = prepared.nextPrimitiveToRealize;
	CesiumGDPreparedPrimitive& preparedPrimitive =
		prepared.primitives[primitiveIndex];
	const CesiumGDPrimitiveInstance& source = preparedPrimitive.source;
	if (
		source.meshIndex < 0 ||
		source.meshIndex >= static_cast<int32_t>(model.meshes.size())
	) {
		*error = Error::ERR_INVALID_DATA;
		return true;
	}
	const CesiumGltf::Mesh& mesh = model.meshes[source.meshIndex];
	if (
		source.primitiveIndex < 0 ||
		source.primitiveIndex >= static_cast<int32_t>(mesh.primitives.size())
	) {
		*error = Error::ERR_INVALID_DATA;
		return true;
	}
	const CesiumGltf::MeshPrimitive& primitive =
		mesh.primitives[source.primitiveIndex];
	const CesiumGltf::Material* gltfMaterial = CesiumGltf::Model::getSafe(
		&model.materials,
		primitive.material
	);

	const bool usesVertexColors = !preparedPrimitive.colors.is_empty();
	const bool usesUnshadedFallback = preparedPrimitive.normals.is_empty();
	const bool isPointPrimitive =
		primitive.mode == CesiumGltf::MeshPrimitive::Mode::POINTS;
	const bool requiresUnculledInstancing =
		preparedPrimitive.requiresUnculledInstancing &&
		(gltfMaterial == nullptr || !gltfMaterial->doubleSided);
	// A glTF material can be shared by primitives whose Godot-only vertex-color
	// or missing-normal state differs. Keep those variants separate while the
	// material loader continues to share their underlying image resources.
	const int64_t materialKey =
		(static_cast<int64_t>(primitive.material) + 1) * 16 +
		(usesVertexColors ? 1 : 0) +
		(usesUnshadedFallback ? 2 : 0) +
		(requiresUnculledInstancing ? 4 : 0) +
		(isPointPrimitive ? 8 : 0);
	if (preparedPrimitive.realizedMaterial.is_null()) {
		const bool requiresPrimitiveMaterial =
			preparedPrimitive.featureStyleEncoding.requested;
		auto existing = prepared.realizedMaterials.find(materialKey);
		if (
			!requiresPrimitiveMaterial &&
			existing != prepared.realizedMaterials.end()
		) {
			preparedPrimitive.realizedMaterial = existing->second;
		} else {
			std::optional<CesiumGltf::Material> unculledMaterial;
			const CesiumGltf::Material* effectiveMaterial = gltfMaterial;
			if (requiresUnculledInstancing) {
				unculledMaterial = gltfMaterial == nullptr
					? CesiumGltf::Material()
					: *gltfMaterial;
				unculledMaterial->doubleSided = true;
				effectiveMaterial = &*unculledMaterial;
			}
			Error materialError = Error::OK;
			const CesiumGltfPrimitiveMaterialOptions materialOptions{
				isPointPrimitive,
				usesUnshadedFallback
			};
			preparedPrimitive.realizedMaterial =
				prepared.materialLoader->create_material(
					effectiveMaterial,
					&preparedPrimitive.featureStyleEncoding,
					materialOptions,
					&materialError
				);
			if (preparedPrimitive.realizedMaterial.is_null()) {
				prepared.realizationFailureStage =
					CesiumGDRealizationFailureStage::Material;
				*error = materialError == Error::OK
					? Error::ERR_CANT_CREATE
					: materialError;
				return true;
			}
			if (materialError != Error::OK) {
				prepared.pendingMaterialWarningError =
					static_cast<int32_t>(materialError);
				WARN_PRINT(
					"glTF material contained an invalid texture; rendering "
					"the remaining valid material properties"
				);
			}
			Ref<StandardMaterial3D> standardMaterial =
				preparedPrimitive.realizedMaterial;
			if (standardMaterial.is_valid()) {
				standardMaterial->set_flag(
					BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR,
					usesVertexColors
				);
				if (usesUnshadedFallback) {
					standardMaterial->set_shading_mode(
						BaseMaterial3D::SHADING_MODE_UNSHADED
					);
				}
			}
			if (!requiresPrimitiveMaterial) {
				prepared.realizedMaterials.emplace(
					materialKey,
					preparedPrimitive.realizedMaterial
				);
			}
			// Material and texture realization is independently indivisible in
			// Godot. Yield before uploading geometry.
			return false;
		}
	}

	if (preparedPrimitive.realizedMesh.is_null()) {
		Array arrays = generate_array_mesh_ext(
			preparedPrimitive.vertices,
			preparedPrimitive.indices,
			preparedPrimitive.normals,
			preparedPrimitive.tangents,
			preparedPrimitive.colors,
			preparedPrimitive.textureCoords,
			preparedPrimitive.textureCoords2,
			preparedPrimitive.encodedFeatureIds
		);
		if (
			preparedPrimitive.isGpuInstanced ||
			preparedPrimitive.isTranslucent
		) {
			preparedPrimitive.realizedMesh.instantiate();
			preparedPrimitive.realizedSurfaceIndex = 0;
		} else {
			preparedPrimitive.realizedMesh = prepared.realizedMesh;
			preparedPrimitive.realizedSurfaceIndex =
				prepared.realizedMesh->get_surface_count();
		}
		*error = apply_surface_to_mesh(
			primitive,
			preparedPrimitive.realizedMesh,
			arrays,
			!preparedPrimitive.encodedFeatureIds.is_empty()
		);
		if (*error != Error::OK) {
			return true;
		}
		preparedPrimitive.realizedMesh->surface_set_material(
			preparedPrimitive.realizedSurfaceIndex,
			preparedPrimitive.realizedMaterial
		);
		return false;
	}

	if (
		preparedPrimitive.isGpuInstanced &&
		preparedPrimitive.realizedMultiMesh.is_null()
	) {
		const bool hasInstanceFeatureIds =
			!preparedPrimitive.encodedInstanceFeatureIds.empty();
		const int64_t outputStride = hasInstanceFeatureIds ? 16 : 12;
		PackedFloat32Array instanceTransformBuffer;
		instanceTransformBuffer.resize(
			static_cast<int64_t>(preparedPrimitive.instanceCount) * outputStride
		);
		float* output = instanceTransformBuffer.ptrw();
		for (int32_t instance = 0;
			instance < preparedPrimitive.instanceCount;
			++instance) {
			const size_t sourceTransform = static_cast<size_t>(instance) * 12;
			const int64_t destination = static_cast<int64_t>(instance) * outputStride;
			std::copy_n(
				preparedPrimitive.instanceTransformBuffer.begin() + sourceTransform,
				12,
				output + destination
			);
			if (hasInstanceFeatureIds) {
				const size_t sourceFeature = static_cast<size_t>(instance) * 4;
				std::copy_n(
					preparedPrimitive.encodedInstanceFeatureIds.begin() + sourceFeature,
					4,
					output + destination + 12
				);
			}
		}
		preparedPrimitive.realizedMultiMesh.instantiate();
		preparedPrimitive.realizedMultiMesh->set_transform_format(
			MultiMesh::TRANSFORM_3D
		);
		preparedPrimitive.realizedMultiMesh->set_use_colors(false);
		preparedPrimitive.realizedMultiMesh->set_use_custom_data(
			hasInstanceFeatureIds
		);
		preparedPrimitive.realizedMultiMesh->set_mesh(
			preparedPrimitive.realizedMesh
		);
		preparedPrimitive.realizedMultiMesh->set_instance_count(
			preparedPrimitive.instanceCount
		);
		preparedPrimitive.realizedMultiMesh->set_buffer(
			instanceTransformBuffer
		);
		return false;
	}

	++prepared.nextPrimitiveToRealize;
	return false;
}

Ref<ArrayMesh> CesiumGDModelLoader::generate_meshes_from_model(
	const CesiumGltf::Model& model,
	Error* error
) {
	std::unique_ptr<CesiumGDPreparedModel> prepared(
		prepare_model(model, error)
	);
	if (prepared == nullptr || *error != Error::OK) {
		return Ref<ArrayMesh>();
	}
	return realize_prepared_model(model, *prepared, error);
}

Dictionary CesiumGDModelLoader::get_texture_coordinate_mappings(
	const CesiumGltf::MeshPrimitive& primitive
) {
	Dictionary mappings;
	if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
		mappings["TEXCOORD_0"] = 0;
	}
	if (
		primitive.attributes.find("_CESIUMOVERLAY_0") != primitive.attributes.end()
	) {
		// Cesium Native may generate the overlay semantic alongside an authored
		// TEXCOORD_0 accessor. Both names then address the same realized Godot UV
		// channel; treating them as mutually exclusive makes raster bindings
		// attach with coordinate index -1 on ordinary textured glTF content.
		mappings["_CESIUMOVERLAY_0"] = 0;
	}

	if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end()) {
		mappings["TEXCOORD_1"] = 1;
	}
	if (
		primitive.attributes.find("_CESIUMOVERLAY_1") != primitive.attributes.end()
	) {
		mappings["_CESIUMOVERLAY_1"] = 1;
	}
	return mappings;
}

Vector<Vector3> CesiumGDModelLoader::get_positions(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	Error* error
) {
	Vector<Vector3> result;
	auto view = CesiumGltf::getPositionAccessorView(model, primitive);
	if (
		std::visit(CesiumGltf::StatusFromAccessor(), view) !=
		CesiumGltf::AccessorViewStatus::Valid
	) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}
	const int64_t count = std::visit(CesiumGltf::CountFromAccessor(), view);
	result.resize(static_cast<int32_t>(count));
	for (int64_t i = 0; i < count; ++i) {
		const std::optional<glm::dvec3> value = std::visit(
			CesiumGltf::PositionFromAccessor{i},
			view
		);
		if (!value.has_value()) {
			*error = Error::ERR_INVALID_DATA;
			return Vector<Vector3>();
		}
		result.set(i, Vector3(value->x, value->y, value->z));
	}
	return result;
}

Vector<Vector3> CesiumGDModelLoader::get_normals(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	Error* error
) {
	Vector<Vector3> result;
	if (primitive.attributes.find("NORMAL") == primitive.attributes.end()) {
		return result;
	}
	auto view = CesiumGltf::getNormalAccessorView(model, primitive);
	if (
		std::visit(CesiumGltf::StatusFromAccessor(), view) !=
		CesiumGltf::AccessorViewStatus::Valid
	) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}
	const int64_t count = std::visit(CesiumGltf::CountFromAccessor(), view);
	result.resize(static_cast<int32_t>(count));
	for (int64_t i = 0; i < count; ++i) {
		const std::optional<glm::dvec3> value = std::visit(
			CesiumGltf::NormalFromAccessor{i},
			view
		);
		if (!value.has_value()) {
			*error = Error::ERR_INVALID_DATA;
			return Vector<Vector3>();
		}
		result.set(i, Vector3(value->x, value->y, value->z));
	}
	return result;
}

Vector<Vector4> CesiumGDModelLoader::get_tangents(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	Error* error
) {
	Vector<Vector4> result;
	const CesiumGltf::Accessor* accessor = get_attribute_accessor(
		primitive,
		model,
		"TANGENT"
	);
	if (accessor == nullptr) {
		return result;
	}
	if (
		accessor->type != CesiumGltf::Accessor::Type::VEC4 ||
		accessor->componentType != CesiumGltf::Accessor::ComponentType::FLOAT
	) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}
	CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC4<float>> view(
		model,
		*accessor
	);
	if (view.status() != CesiumGltf::AccessorViewStatus::Valid) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}
	result.resize(static_cast<int32_t>(view.size()));
	for (int64_t i = 0; i < view.size(); ++i) {
		const auto& value = view[i].value;
		result.set(i, Vector4(value[0], value[1], value[2], value[3]));
	}
	return result;
}

Vector<Color> CesiumGDModelLoader::get_colors(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	Error* error
) {
	Vector<Color> result;
	const CesiumGltf::Accessor* accessor = get_attribute_accessor(
		primitive,
		model,
		"COLOR_0"
	);
	if (accessor == nullptr) {
		return result;
	}
	const bool vec3 = accessor->type == CesiumGltf::Accessor::Type::VEC3;
	const bool vec4 = accessor->type == CesiumGltf::Accessor::Type::VEC4;
	if (!vec3 && !vec4) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}

	auto load = [&result, error](const auto& view, bool hasAlpha, bool normalized) {
		if (view.status() != CesiumGltf::AccessorViewStatus::Valid) {
			*error = Error::ERR_INVALID_DATA;
			return;
		}
		result.resize(static_cast<int32_t>(view.size()));
		for (int64_t i = 0; i < view.size(); ++i) {
			const auto& source = view[i].value;
			using Component = std::remove_cv_t<
				std::remove_reference_t<decltype(source[0])>
			>;
			auto convert = [normalized](Component value) -> double {
				if constexpr (std::is_floating_point_v<Component>) {
					return static_cast<double>(value);
				} else {
					return normalized ? normalized_unsigned(value) : 0.0;
				}
			};
			result.set(
				i,
				Color(
					convert(source[0]),
					convert(source[1]),
					convert(source[2]),
					hasAlpha ? convert(source[3]) : 1.0
				)
			);
		}
	};

	switch (accessor->componentType) {
	case CesiumGltf::Accessor::ComponentType::FLOAT:
		if (vec4) {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC4<float>>(model, *accessor), true, true);
		} else {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<float>>(model, *accessor), false, true);
		}
		break;
	case CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE:
		if (!accessor->normalized) {
			*error = Error::ERR_INVALID_DATA;
			break;
		}
		if (vec4) {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC4<uint8_t>>(model, *accessor), true, true);
		} else {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<uint8_t>>(model, *accessor), false, true);
		}
		break;
	case CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT:
		if (!accessor->normalized) {
			*error = Error::ERR_INVALID_DATA;
			break;
		}
		if (vec4) {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC4<uint16_t>>(model, *accessor), true, true);
		} else {
			load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC3<uint16_t>>(model, *accessor), false, true);
		}
		break;
	default:
		*error = Error::ERR_INVALID_DATA;
		break;
	}
	return result;
}

Vector<Vector2> CesiumGDModelLoader::get_texture_coordinates(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	const std::string& semantic,
	Error* error
) {
	Vector<Vector2> result;
	const CesiumGltf::Accessor* accessor = get_attribute_accessor(
		primitive,
		model,
		semantic
	);
	if (accessor == nullptr) {
		return result;
	}
	if (accessor->type != CesiumGltf::Accessor::Type::VEC2) {
		*error = Error::ERR_INVALID_DATA;
		return result;
	}

	auto load = [&result, error](const auto& view, bool normalized) {
		if (view.status() != CesiumGltf::AccessorViewStatus::Valid) {
			*error = Error::ERR_INVALID_DATA;
			return;
		}
		result.resize(static_cast<int32_t>(view.size()));
		for (int64_t i = 0; i < view.size(); ++i) {
			const auto& source = view[i].value;
			using Component = std::remove_cv_t<
				std::remove_reference_t<decltype(source[0])>
			>;
			auto convert = [normalized](Component value) -> double {
				if constexpr (std::is_floating_point_v<Component>) {
					return static_cast<double>(value);
				} else {
					return normalized ? normalized_unsigned(value) : 0.0;
				}
			};
			result.set(i, Vector2(convert(source[0]), convert(source[1])));
		}
	};

	switch (accessor->componentType) {
	case CesiumGltf::Accessor::ComponentType::FLOAT:
		load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<float>>(model, *accessor), true);
		break;
	case CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE:
		if (!accessor->normalized) {
			*error = Error::ERR_INVALID_DATA;
			break;
		}
		load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint8_t>>(model, *accessor), true);
		break;
	case CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT:
		if (!accessor->normalized) {
			*error = Error::ERR_INVALID_DATA;
			break;
		}
		load(CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint16_t>>(model, *accessor), true);
		break;
	default:
		*error = Error::ERR_INVALID_DATA;
		break;
	}
	return result;
}

Vector<int32_t> CesiumGDModelLoader::get_index_buffer_from_primitive(
	const CesiumGltf::MeshPrimitive& primitive,
	const CesiumGltf::Model& model,
	int32_t vertexCount,
	Error* error
) {
	Vector<int32_t> source;
	if (primitive.indices < 0) {
		source.resize(vertexCount);
		for (int32_t i = 0; i < vertexCount; ++i) {
			source.set(i, i);
		}
	} else {
		CesiumGltf::IndexAccessorType accessor =
			CesiumGltf::getIndexAccessorView(model, primitive);
		if (
			std::visit(CesiumGltf::StatusFromAccessor(), accessor) !=
			CesiumGltf::AccessorViewStatus::Valid
		) {
			*error = Error::ERR_INVALID_DATA;
			return source;
		}
		const int64_t count = std::visit(CesiumGltf::CountFromAccessor(), accessor);
		source.resize(static_cast<int32_t>(count));
		for (int64_t i = 0; i < count; ++i) {
			const int64_t index = std::visit(
				CesiumGltf::IndexFromAccessor{i},
				accessor
			);
			if (index < 0 || index >= vertexCount) {
				*error = Error::ERR_INVALID_DATA;
				return Vector<int32_t>();
			}
			source.set(static_cast<int32_t>(i), static_cast<int32_t>(index));
		}
	}

	Vector<int32_t> result;
	if (primitive.mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN) {
		if (source.size() < 3) {
			*error = Error::ERR_INVALID_DATA;
			return result;
		}
		result.resize((source.size() - 2) * 3);
		int32_t output = 0;
		for (int32_t i = 1; i + 1 < source.size(); ++i) {
			result.set(output++, source[0]);
			result.set(output++, source[i]);
			result.set(output++, source[i + 1]);
		}
	} else if (
		primitive.mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP
	) {
		if (source.size() < 3) {
			*error = Error::ERR_INVALID_DATA;
			return result;
		}
		result.resize((source.size() - 2) * 3);
		int32_t output = 0;
		for (int32_t i = 0; i + 2 < source.size(); ++i) {
			const bool odd = (i & 1) != 0;
			result.set(output++, source[i + (odd ? 1 : 0)]);
			result.set(output++, source[i + (odd ? 0 : 1)]);
			result.set(output++, source[i + 2]);
		}
	} else if (primitive.mode == CesiumGltf::MeshPrimitive::Mode::LINE_LOOP) {
		if (source.size() < 2) {
			*error = Error::ERR_INVALID_DATA;
			return result;
		}
		result.resize(source.size() * 2);
		for (int32_t i = 0; i < source.size(); ++i) {
			result.set(i * 2, source[i]);
			result.set(i * 2 + 1, source[(i + 1) % source.size()]);
		}
	} else {
		result = source;
	}

	if (
		primitive.mode == CesiumGltf::MeshPrimitive::Mode::TRIANGLES &&
		result.size() % 3 != 0
	) {
		*error = Error::ERR_INVALID_DATA;
		return Vector<int32_t>();
	}
	if (
		primitive.mode == CesiumGltf::MeshPrimitive::Mode::LINES &&
		result.size() % 2 != 0
	) {
		*error = Error::ERR_INVALID_DATA;
		return Vector<int32_t>();
	}
	return result;
}

Error CesiumGDModelLoader::validate_attribute_count(
	int32_t vertexCount,
	int32_t attributeCount,
	const char* semantic
) {
	if (attributeCount != 0 && attributeCount != vertexCount) {
		ERR_PRINT(
			String("glTF ") + semantic + " count does not match POSITION count"
		);
		return Error::ERR_INVALID_DATA;
	}
	return Error::OK;
}

Error CesiumGDModelLoader::generate_normals(
	Vector<Vector3>* normalBuffer,
	const Vector<Vector3>& vertices,
	const Vector<int32_t>& indices
) {
	ERR_FAIL_NULL_V(normalBuffer, Error::ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!normalBuffer->is_empty(), Error::ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(indices.size() % 3 != 0, Error::ERR_INVALID_DATA);
	normalBuffer->resize(vertices.size());
	for (int32_t i = 0; i < normalBuffer->size(); ++i) {
		normalBuffer->set(i, Vector3());
	}
	for (int32_t i = 0; i < indices.size(); i += 3) {
		const int32_t a = indices[i];
		const int32_t b = indices[i + 1];
		const int32_t c = indices[i + 2];
		if (
			a < 0 || b < 0 || c < 0 ||
			a >= vertices.size() || b >= vertices.size() || c >= vertices.size()
		) {
			return Error::ERR_INVALID_DATA;
		}
		const Vector3 weighted =
			(vertices[b] - vertices[a]).cross(vertices[c] - vertices[a]);
		normalBuffer->set(a, (*normalBuffer)[a] + weighted);
		normalBuffer->set(b, (*normalBuffer)[b] + weighted);
		normalBuffer->set(c, (*normalBuffer)[c] + weighted);
	}
	for (int32_t i = 0; i < normalBuffer->size(); ++i) {
		normalBuffer->set(i, (*normalBuffer)[i].normalized());
	}
	return Error::OK;
}

#if defined(CESIUM_GD_EXT)
Array CesiumGDModelLoader::generate_array_mesh_ext(
	const Vector<Vector3>& vertices,
	const Vector<int32_t>& indices,
	const Vector<Vector3>& normals,
	const Vector<Vector4>& tangents,
	const Vector<Color>& colors,
	const Vector<Vector2>& textureCoords,
	const Vector<Vector2>& textureCoords2,
	const Vector<Vector3>& encodedFeatureIds
) {
	Array arrays;
	arrays.resize(ArrayMesh::ARRAY_MAX);

	PackedVector3Array packedVertices;
	packedVertices.resize(vertices.size());
	for (int32_t i = 0; i < vertices.size(); ++i) {
		packedVertices.set(i, vertices[i]);
	}
	arrays[ArrayMesh::ARRAY_VERTEX] = packedVertices;

	if (!indices.is_empty()) {
		PackedInt32Array packedIndices;
		packedIndices.resize(indices.size());
		for (int32_t i = 0; i < indices.size(); ++i) {
			packedIndices.set(i, indices[i]);
		}
		arrays[ArrayMesh::ARRAY_INDEX] = packedIndices;
	}
	if (!normals.is_empty()) {
		PackedVector3Array packedNormals;
		packedNormals.resize(normals.size());
		for (int32_t i = 0; i < normals.size(); ++i) {
			packedNormals.set(i, normals[i]);
		}
		arrays[ArrayMesh::ARRAY_NORMAL] = packedNormals;
	}
	if (!tangents.is_empty()) {
		PackedFloat32Array packedTangents;
		packedTangents.resize(tangents.size() * 4);
		for (int32_t i = 0; i < tangents.size(); ++i) {
			packedTangents.set(i * 4, tangents[i].x);
			packedTangents.set(i * 4 + 1, tangents[i].y);
			packedTangents.set(i * 4 + 2, tangents[i].z);
			packedTangents.set(i * 4 + 3, tangents[i].w);
		}
		arrays[ArrayMesh::ARRAY_TANGENT] = packedTangents;
	}
	if (!colors.is_empty()) {
		PackedColorArray packedColors;
		packedColors.resize(colors.size());
		for (int32_t i = 0; i < colors.size(); ++i) {
			packedColors.set(i, colors[i]);
		}
		arrays[ArrayMesh::ARRAY_COLOR] = packedColors;
	}
	if (!textureCoords.is_empty()) {
		PackedVector2Array packed;
		packed.resize(textureCoords.size());
		for (int32_t i = 0; i < textureCoords.size(); ++i) {
			packed.set(i, textureCoords[i]);
		}
		arrays[ArrayMesh::ARRAY_TEX_UV] = packed;
	}
	if (!textureCoords2.is_empty()) {
		PackedVector2Array packed;
		packed.resize(textureCoords2.size());
		for (int32_t i = 0; i < textureCoords2.size(); ++i) {
			packed.set(i, textureCoords2[i]);
		}
		arrays[ArrayMesh::ARRAY_TEX_UV2] = packed;
	}
	if (!encodedFeatureIds.is_empty()) {
		PackedFloat32Array packed;
		packed.resize(static_cast<int64_t>(encodedFeatureIds.size()) * 3);
		for (int32_t i = 0; i < encodedFeatureIds.size(); ++i) {
			packed.set(i * 3, encodedFeatureIds[i].x);
			packed.set(i * 3 + 1, encodedFeatureIds[i].y);
			packed.set(i * 3 + 2, encodedFeatureIds[i].z);
		}
		arrays[ArrayMesh::ARRAY_CUSTOM0] = packed;
	}
	return arrays;
}
#endif

constexpr Mesh::PrimitiveType CesiumGDModelLoader::cesium_to_godot_primitive_mode(
	int32_t mode
) {
	switch (mode) {
	case CesiumGltf::MeshPrimitive::Mode::POINTS:
		return Mesh::PRIMITIVE_POINTS;
	case CesiumGltf::MeshPrimitive::Mode::LINES:
	case CesiumGltf::MeshPrimitive::Mode::LINE_LOOP:
		return Mesh::PRIMITIVE_LINES;
	case CesiumGltf::MeshPrimitive::Mode::LINE_STRIP:
		return Mesh::PRIMITIVE_LINE_STRIP;
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP:
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN:
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLES:
	default:
		return Mesh::PRIMITIVE_TRIANGLES;
	}
}

Error CesiumGDModelLoader::apply_surface_to_mesh(
	const CesiumGltf::MeshPrimitive& primitive,
	Ref<ArrayMesh>& mesh,
	const Array& arrays,
	bool hasEncodedFeatureIds
) {
	BitField<Mesh::ArrayFormat> flags(0);
	if (hasEncodedFeatureIds) {
		flags = BitField<Mesh::ArrayFormat>(
			static_cast<uint64_t>(Mesh::ARRAY_CUSTOM_RGB_FLOAT) <<
			Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT
		);
	}
	mesh->add_surface_from_arrays(
		cesium_to_godot_primitive_mode(primitive.mode),
		arrays,
		TypedArray<Array>(),
		Dictionary(),
		flags
	);
	return Error::OK;
}

glm::dmat4 CesiumGDModelLoader::apply_rtc_center(
	const CesiumGltf::Model& gltf,
	const glm::dmat4x4& rootTransform
) {
	const CesiumGltf::ExtensionCesiumRTC* cesiumRTC =
		gltf.getExtension<CesiumGltf::ExtensionCesiumRTC>();
	if (cesiumRTC == nullptr || cesiumRTC->center.size() != 3) {
		return rootTransform;
	}
	const glm::dmat4 rtcTransform(
		glm::dvec4(1.0, 0.0, 0.0, 0.0),
		glm::dvec4(0.0, 1.0, 0.0, 0.0),
		glm::dvec4(0.0, 0.0, 1.0, 0.0),
		glm::dvec4(
			cesiumRTC->center[0],
			cesiumRTC->center[1],
			cesiumRTC->center[2],
			1.0
		)
	);
	return rootTransform * rtcTransform;
}

glm::dmat4 CesiumGDModelLoader::apply_gltf_up_axis_transform(
	const CesiumGltf::Model& model,
	const glm::dmat4x4& rootTransform
) {
	auto gltfUpAxis = model.extras.find("gltfUpAxis");
	if (gltfUpAxis == model.extras.end()) {
		return rootTransform * CesiumGeometry::Transforms::Y_UP_TO_Z_UP;
	}
	const int value = static_cast<int>(
		gltfUpAxis->second.getSafeNumberOrDefault(1)
	);
	if (value == static_cast<int>(CesiumGeometry::Axis::X)) {
		return rootTransform * CesiumGeometry::Transforms::X_UP_TO_Z_UP;
	}
	if (value == static_cast<int>(CesiumGeometry::Axis::Y)) {
		return rootTransform * CesiumGeometry::Transforms::Y_UP_TO_Z_UP;
	}
	return rootTransform;
}

Error CesiumGDModelLoader::parse_gltf(
	const String& assetPath,
	CesiumGltfReader::GltfReaderResult* out
) {
	ERR_FAIL_NULL_V(out, Error::ERR_INVALID_PARAMETER);
	Error error = Error::OK;
	Ref<FileAccess> asset = open_file_access_with_err(
		assetPath,
		FileAccess::READ,
		&error
	);
	if (error != Error::OK) {
		ERR_PRINT(String("Error loading glTF from disk: ") + REFLECT_ERR_NAME(error));
		return error;
	}

	PackedByteArray bytes = asset->get_buffer(asset->get_length());
	const std::byte* data = reinterpret_cast<const std::byte*>(bytes.ptr());
	CesiumGltfReader::GltfReader reader;
	CesiumGltfReader::GltfReaderOptions options{};
	// Preserve KHR_texture_transform for the Godot material adapter. Applying
	// it destructively to shared UV accessors cannot represent two textures
	// that use different transforms over the same source UV set.
	options.applyTextureTransform = false;
	options.decodeDraco = true;
	options.decodeMeshOptData = true;
	options.ktx2TranscodeTargets = CesiumImage::Ktx2TranscodeTargets(
		CesiumGDTextureLoader::get_supported_gpu_compressed_pixel_formats(),
		true
	);
	*out = reader.readGltf(
		std::span<const std::byte>(data, static_cast<size_t>(bytes.size())),
		options
	);
	if (!out->errors.empty() || !out->model) {
		return Error::ERR_FILE_CORRUPT;
	}
	return Error::OK;
}
