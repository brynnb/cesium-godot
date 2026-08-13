// Copyright 2020-2026 CesiumGS, Inc. and Contributors
// Adapted for Godot from Cesium for Unreal under the Apache-2.0 license.

#include "Runtime/Public/Renderer/CesiumGltfInstancedComponent.h"

#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"

void CesiumGltfInstancedComponent::initialize(
	int32_t rendererPrimitiveIndex,
	const Ref<MultiMesh>& multiMesh,
	const AABB& instanceBounds,
	const Transform3D& nodeTransform,
	const Ref<CesiumPrimitiveFeatures>& instanceFeatures
) {
	this->m_rendererPrimitiveIndex = rendererPrimitiveIndex;
	this->set_multimesh(multiMesh);
	this->set_custom_aabb(instanceBounds);
	this->set_transform(nodeTransform);
	this->m_instanceFeatures = instanceFeatures;
}

Ref<CesiumPrimitiveFeatures>
CesiumGltfInstancedComponent::get_instance_features() const {
	return this->m_instanceFeatures;
}

int32_t CesiumGltfInstancedComponent::get_renderer_primitive_index() const {
	return this->m_rendererPrimitiveIndex;
}

int32_t CesiumGltfInstancedComponent::get_instance_count() const {
	const Ref<MultiMesh> multiMesh = this->get_multimesh();
	return multiMesh.is_valid() ? multiMesh->get_instance_count() : 0;
}

void CesiumGltfInstancedComponent::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("get_renderer_primitive_index"),
		&CesiumGltfInstancedComponent::get_renderer_primitive_index
	);
	ClassDB::bind_method(
		D_METHOD("get_instance_count"),
		&CesiumGltfInstancedComponent::get_instance_count
	);
	ClassDB::bind_method(
		D_METHOD("get_instance_features"),
		&CesiumGltfInstancedComponent::get_instance_features
	);

	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"renderer_primitive_index",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_renderer_primitive_index"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"instance_count",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_instance_count"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"instance_features",
			PROPERTY_HINT_RESOURCE_TYPE,
			"CesiumPrimitiveFeatures",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_instance_features"
	);
}
