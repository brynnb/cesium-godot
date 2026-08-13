#include "Runtime/Public/CesiumLoadedTilePrimitive.h"

#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "Runtime/Public/Renderer/CesiumGltfInstancedComponent.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Utils/CesiumMathUtils.h"

#include "CesiumGltf/MeshPrimitive.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/classes/shader.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#endif

namespace {
void split_vector_for_shader(
	const Vector3& value,
	Vector3* high,
	Vector3* low
) {
	const float highX = static_cast<float>(value.x);
	const float highY = static_cast<float>(value.y);
	const float highZ = static_cast<float>(value.z);
	*high = Vector3(highX, highY, highZ);
	*low = Vector3(
		value.x - static_cast<real_t>(highX),
		value.y - static_cast<real_t>(highY),
		value.z - static_cast<real_t>(highZ)
	);
}

Ref<Material> resolve_material(const ObjectID& id) {
	if (id.is_null()) {
		return Ref<Material>();
	}
	Material* material = Object::cast_to<Material>(ObjectDB::get_instance(id));
	return material == nullptr ? Ref<Material>() : Ref<Material>(material);
}

Dictionary describe_material(const Ref<Material>& material) {
	Dictionary result;
	result["available"] = material.is_valid();
	if (material.is_null()) {
		return result;
	}
	result["instance_id"] = static_cast<int64_t>(material->get_instance_id());
	result["class_name"] = String(material->get_class());
	result["name"] = material->get_name();
	result["resource_path"] = material->get_path();
	result["material_source"] = material->get_meta(
		"cesium_material_source",
		String()
	);
	result["fallback_reason"] = material->get_meta(
		"cesium_material_fallback_reason",
		String()
	);
	result["sampler_exact"] = material->get_meta(
		"cesium_sampler_exact",
		true
	);
	result["unsupported_extensions"] = material->get_meta(
		"cesium_unsupported_material_extensions",
		PackedStringArray()
	);
	result["render_priority"] = material->get_render_priority();
	result["alpha_mode"] = material->get_meta(
		"cesium_alpha_mode",
		"UNKNOWN"
	);
	result["source_translucent"] = material->get_meta(
		"cesium_source_translucent",
		false
	);
	result["translucency_depth_prepass"] = material->get_meta(
		"cesium_translucency_depth_prepass",
		false
	);
	result["lod_transition_support_known"] = material->has_meta(
		"cesium_lod_transition_supported"
	);
	result["lod_transition_supported"] = material->get_meta(
		"cesium_lod_transition_supported",
		false
	);
	Ref<ShaderMaterial> shaderMaterial = material;
	if (shaderMaterial.is_valid()) {
		Ref<Shader> shader = shaderMaterial->get_shader();
		result["shader_available"] = shader.is_valid();
		if (shader.is_valid()) {
			result["shader_instance_id"] = static_cast<int64_t>(
				shader->get_instance_id()
			);
			result["shader_name"] = shader->get_name();
			result["shader_resource_path"] = shader->get_path();
		}
	}
	return result;
}

Dictionary describe_texture(const Ref<Texture2D>& texture) {
	Dictionary result;
	result["available"] = texture.is_valid();
	if (texture.is_null()) {
		return result;
	}
	result["instance_id"] = static_cast<int64_t>(texture->get_instance_id());
	result["class_name"] = String(texture->get_class());
	result["name"] = texture->get_name();
	result["resource_path"] = texture->get_path();
	result["width"] = texture->get_width();
	result["height"] = texture->get_height();
	return result;
}
}

Cesium3DTile* CesiumLoadedTilePrimitive::get_loaded_tile() const {
	if (this->m_tile.is_null()) {
		return nullptr;
	}
	return Object::cast_to<Cesium3DTile>(ObjectDB::get_instance(this->m_tile));
}

GeometryInstance3D* CesiumLoadedTilePrimitive::get_render_node() const {
	if (this->m_renderNode.is_null()) {
		return nullptr;
	}
	return Object::cast_to<GeometryInstance3D>(
		ObjectDB::get_instance(this->m_renderNode)
	);
}

String CesiumLoadedTilePrimitive::get_tile_id() const {
	return this->m_tileId;
}

Variant CesiumLoadedTilePrimitive::get_tile_extras() const {
	return this->m_tileExtras.duplicate(true);
}

int32_t CesiumLoadedTilePrimitive::get_surface_index() const {
	return this->m_surfaceIndex;
}

int32_t CesiumLoadedTilePrimitive::get_mesh_surface_index() const {
	return this->m_meshSurfaceIndex;
}

bool CesiumLoadedTilePrimitive::is_gpu_instanced() const {
	return this->m_gpuInstanced;
}

int32_t CesiumLoadedTilePrimitive::get_instance_count() const {
	return this->m_instanceCount;
}

int32_t CesiumLoadedTilePrimitive::get_node_index() const {
	return this->m_nodeIndex;
}

int32_t CesiumLoadedTilePrimitive::get_mesh_index() const {
	return this->m_meshIndex;
}

int32_t CesiumLoadedTilePrimitive::get_primitive_index() const {
	return this->m_primitiveIndex;
}

int32_t CesiumLoadedTilePrimitive::get_material_index() const {
	return this->m_materialIndex;
}

int32_t CesiumLoadedTilePrimitive::get_primitive_mode() const {
	return this->m_primitiveMode;
}

String CesiumLoadedTilePrimitive::get_primitive_mode_name() const {
	switch (this->m_primitiveMode) {
	case CesiumGltf::MeshPrimitive::Mode::POINTS:
		return "points";
	case CesiumGltf::MeshPrimitive::Mode::LINES:
		return "lines";
	case CesiumGltf::MeshPrimitive::Mode::LINE_LOOP:
		return "line_loop";
	case CesiumGltf::MeshPrimitive::Mode::LINE_STRIP:
		return "line_strip";
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLES:
		return "triangles";
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP:
		return "triangle_strip";
	case CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN:
		return "triangle_fan";
	default:
		return "unknown";
	}
}

bool CesiumLoadedTilePrimitive::is_point_primitive() const {
	return this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::POINTS;
}

bool CesiumLoadedTilePrimitive::is_line_primitive() const {
	return this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::LINES ||
		this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::LINE_LOOP ||
		this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::LINE_STRIP;
}

bool CesiumLoadedTilePrimitive::is_triangle_primitive() const {
	return this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::TRIANGLES ||
		this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_STRIP ||
		this->m_primitiveMode == CesiumGltf::MeshPrimitive::Mode::TRIANGLE_FAN;
}

bool CesiumLoadedTilePrimitive::is_translucent() const {
	return this->m_translucent;
}

bool CesiumLoadedTilePrimitive::is_translucency_isolated() const {
	return this->m_translucencyIsolated;
}

int64_t CesiumLoadedTilePrimitive::get_point_count() const {
	return this->m_pointCount;
}

int64_t CesiumLoadedTilePrimitive::get_point_diameter() const {
	return this->m_pointDiameter;
}

Vector3 CesiumLoadedTilePrimitive::get_primitive_dimensions() const {
	return this->m_primitiveDimensions;
}

double CesiumLoadedTilePrimitive::get_tile_geometric_error() const {
	return this->m_tileGeometricError;
}

bool CesiumLoadedTilePrimitive::get_uses_additive_refinement() const {
	return this->m_usesAdditiveRefinement;
}

Dictionary CesiumLoadedTilePrimitive::get_point_cloud_parameters() const {
	const Ref<Material> activeMaterial = this->get_active_material();
	if (activeMaterial.is_null()) {
		return Dictionary();
	}
	const Variant parameters = activeMaterial->get_meta(
		"cesium_point_cloud_parameters",
		Dictionary()
	);
	return parameters.get_type() == Variant::DICTIONARY
		? static_cast<Dictionary>(parameters)
		: Dictionary();
}

Dictionary CesiumLoadedTilePrimitive::get_attributes() const {
	return this->m_attributes;
}

Dictionary CesiumLoadedTilePrimitive::get_gltf_material() const {
	return this->m_gltfMaterial;
}

Dictionary CesiumLoadedTilePrimitive::get_texture_coordinate_mappings() const {
	return this->m_textureCoordinateMappings;
}

Ref<Material> CesiumLoadedTilePrimitive::get_default_material() const {
	return resolve_material(this->m_defaultMaterial);
}

Ref<Material> CesiumLoadedTilePrimitive::get_selected_base_material() const {
	return resolve_material(this->m_selectedBaseMaterial);
}

Ref<Material> CesiumLoadedTilePrimitive::get_active_material() const {
	GeometryInstance3D* renderNode = this->get_render_node();
	if (renderNode == nullptr) {
		return Ref<Material>();
	}
	Ref<Material> material = renderNode->get_material_override();
	if (material.is_valid()) {
		return material;
	}
	Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(renderNode);
	if (tile != nullptr && this->m_meshSurfaceIndex >= 0) {
		material = tile->get_surface_override_material(this->m_meshSurfaceIndex);
		if (material.is_valid()) {
			return material;
		}
	}
	return this->get_selected_base_material();
}

bool CesiumLoadedTilePrimitive::get_uses_custom_material() const {
	return this->m_usesCustomMaterial;
}

Array CesiumLoadedTilePrimitive::get_raster_overlay_bindings() const {
	Cesium3DTile* tile = this->get_loaded_tile();
	return tile == nullptr
		? Array()
		: tile->get_raster_overlay_bindings(this->m_surfaceIndex);
}

Dictionary CesiumLoadedTilePrimitive::get_render_diagnostics() const {
	Dictionary result;
	const bool loadedTileAvailable = this->get_loaded_tile() != nullptr;
	const bool renderNodeAvailable = this->get_render_node() != nullptr;
	const Ref<Material> defaultMaterial = this->get_default_material();
	const Ref<Material> selectedMaterial = this->get_selected_base_material();
	const Ref<Material> activeMaterial = this->get_active_material();
	result["tile_id"] = this->m_tileId;
	result["surface_index"] = this->m_surfaceIndex;
	result["mesh_surface_index"] = this->m_meshSurfaceIndex;
	result["material_index"] = this->m_materialIndex;
	result["primitive_mode"] = this->m_primitiveMode;
	result["primitive_mode_name"] = this->get_primitive_mode_name();
	result["point_count"] = this->m_pointCount;
	result["point_diameter"] = this->m_pointDiameter;
	result["primitive_dimensions"] = this->m_primitiveDimensions;
	result["tile_geometric_error"] = this->m_tileGeometricError;
	result["uses_additive_refinement"] = this->m_usesAdditiveRefinement;
	result["translucent"] = this->m_translucent;
	result["translucency_isolated"] = this->m_translucencyIsolated;
	GeometryInstance3D* renderNode = this->get_render_node();
	result["sorting_uses_aabb_center"] = renderNode != nullptr &&
		renderNode->is_sorting_use_aabb_center();
	result["point_cloud_parameters"] = this->get_point_cloud_parameters();
	result["source_material"] = this->m_gltfMaterial.duplicate(true);
	result["source_attributes"] = this->m_attributes.duplicate(true);
	result["texture_coordinate_mappings"] =
		this->m_textureCoordinateMappings.duplicate(true);
	result["default_material"] = describe_material(defaultMaterial);
	result["selected_base_material"] = describe_material(selectedMaterial);
	result["active_material"] = describe_material(activeMaterial);
	result["uses_custom_material"] = this->m_usesCustomMaterial;
	result["loaded_tile_available"] = loadedTileAvailable;
	result["render_node_available"] = renderNodeAvailable;
	result["render_state"] = !loadedTileAvailable
		? String("unloaded")
		: renderNodeAvailable && activeMaterial.is_valid()
			? String("realized")
			: String("renderer_unavailable");

	const Array overlayBindings = this->get_raster_overlay_bindings();
	result["active_material_origin"] = activeMaterial.is_null()
		? String("unavailable")
		: !overlayBindings.is_empty()
		? String("raster_overlay_composite")
		: this->m_usesCustomMaterial
			? String("lifecycle_custom_material")
			: String("generated_gltf_material");
	const bool sourceMaterialDefined =
		this->m_gltfMaterial.has("defined") &&
		static_cast<bool>(this->m_gltfMaterial["defined"]);
	String fallbackReason = sourceMaterialDefined
		? String()
		: String("missing_gltf_material");
	if (defaultMaterial.is_valid()) {
		const String materialFallback = defaultMaterial->get_meta(
			"cesium_material_fallback_reason",
			String()
		);
		if (!materialFallback.is_empty()) {
			fallbackReason = materialFallback;
		}
	}
	result["fallback_active"] = !fallbackReason.is_empty();
	result["fallback_reason"] = fallbackReason;
	Dictionary metadataStyle;
	const Ref<Material> encodingMaterial = defaultMaterial.is_valid()
		? defaultMaterial
		: activeMaterial;
	metadataStyle["requested"] = encodingMaterial.is_valid() &&
		static_cast<bool>(encodingMaterial->get_meta(
			"cesium_feature_style_requested",
			false
		));
	metadataStyle["encoded"] = encodingMaterial.is_valid() &&
		static_cast<bool>(encodingMaterial->get_meta(
			"cesium_feature_style_encoded",
			false
		));
	metadataStyle["source"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta("cesium_feature_style_source", "none")
		: Variant("none");
	metadataStyle["feature_id_set_index"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta(
			"cesium_feature_style_feature_id_set_index",
			-1
		)
		: Variant(-1);
	metadataStyle["feature_id_set_name"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta(
			"cesium_feature_style_feature_id_set_name",
			""
		)
		: Variant("");
	metadataStyle["feature_count"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta(
			"cesium_feature_style_feature_count",
			0
		)
		: Variant(0);
	metadataStyle["property_table_index"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta(
			"cesium_feature_style_property_table_index",
			-1
		)
		: Variant(-1);
	metadataStyle["encoding_diagnostic"] = encodingMaterial.is_valid()
		? encodingMaterial->get_meta(
			"cesium_feature_style_encoding_diagnostic",
			""
		)
		: Variant("material unavailable");
	metadataStyle["applied"] = activeMaterial.is_valid() &&
		static_cast<bool>(activeMaterial->get_meta(
			"cesium_metadata_style_applied",
			false
		));
	metadataStyle["error"] = activeMaterial.is_valid()
		? activeMaterial->get_meta("cesium_metadata_style_error", "")
		: Variant("material unavailable");
	result["metadata_style"] = metadataStyle;

	Ref<ShaderMaterial> generatedShaderMaterial = defaultMaterial;
	Array textureSlots;
	auto addTextureSlot = [&](const String& role, const Dictionary& source) {
		Dictionary slot;
		slot["role"] = role;
		const bool sourceDefined = !source.is_empty();
		slot["source_texture_defined"] = sourceDefined;
		const int32_t textureIndex = sourceDefined && source.has("index")
			? static_cast<int32_t>(source["index"])
			: -1;
		const int32_t sourceUvSet = sourceDefined && source.has("effective_texcoord")
			? static_cast<int32_t>(source["effective_texcoord"])
			: 0;
		const String sourceSemantic =
			"TEXCOORD_" + String::num_int64(sourceUvSet);
		const int32_t godotUvIndex =
			this->m_textureCoordinateMappings.has(sourceSemantic)
				? static_cast<int32_t>(
					this->m_textureCoordinateMappings[sourceSemantic]
				)
				: -1;
		slot["source_texture_index"] = textureIndex;
		slot["source_uv_set"] = sourceUvSet;
		slot["source_semantic"] = sourceSemantic;
		slot["godot_uv_index"] = godotUvIndex;
		Variant activeTexture;
		Variant shaderUvSet;
		if (generatedShaderMaterial.is_valid()) {
			activeTexture = generatedShaderMaterial->get_shader_parameter(
				role + String("_texture")
			);
			shaderUvSet = generatedShaderMaterial->get_shader_parameter(
				role + String("_uv_set")
			);
		}
		Ref<Texture2D> texture = activeTexture;
		slot["generated_texture"] = describe_texture(texture);
		slot["generated_shader_uv_set"] = shaderUvSet;
		String slotFallback;
		if (sourceDefined && godotUvIndex < 0) {
			slotFallback = "source_uv_set_not_realized";
		} else if (sourceDefined && texture.is_null()) {
			slotFallback = "texture_unavailable";
		}
		slot["fallback_reason"] = slotFallback;
		textureSlots.push_back(slot);
	};
	Dictionary pbr;
	if (this->m_gltfMaterial.has("pbr_metallic_roughness")) {
		pbr = this->m_gltfMaterial["pbr_metallic_roughness"];
	}
	addTextureSlot(
		"base_color",
		pbr.has("base_color_texture")
			? static_cast<Dictionary>(pbr["base_color_texture"])
			: Dictionary()
	);
	addTextureSlot(
		"metallic_roughness",
		pbr.has("metallic_roughness_texture")
			? static_cast<Dictionary>(pbr["metallic_roughness_texture"])
			: Dictionary()
	);
	for (const String& role : {
		String("normal"),
		String("occlusion"),
		String("emissive")
	}) {
		const String sourceKey = role + String("_texture");
		addTextureSlot(
			role,
			this->m_gltfMaterial.has(sourceKey)
				? static_cast<Dictionary>(this->m_gltfMaterial[sourceKey])
				: Dictionary()
		);
	}
	result["texture_slots"] = textureSlots;

	Array overlayDiagnostics;
	for (int32_t index = 0; index < overlayBindings.size(); ++index) {
		Ref<CesiumRasterOverlayBinding> binding = overlayBindings[index];
		if (binding.is_null()) {
			continue;
		}
		Dictionary overlay;
		overlay["overlay_key"] = binding->get_overlay_key();
		overlay["attached"] = binding->is_attached();
		overlay["active"] =
			binding->is_attached() && binding->get_material() == activeMaterial;
		overlay["texture_coordinate_id"] =
			binding->get_texture_coordinate_id();
		overlay["texture_coordinate_index"] =
			binding->get_texture_coordinate_index();
		overlay["source_semantic"] =
			"_CESIUMOVERLAY_" + String::num_int64(
				binding->get_texture_coordinate_id()
			);
		overlay["translation"] = binding->get_translation();
		overlay["scale"] = binding->get_scale();
		overlay["shader_parameter_prefix"] =
			binding->get_shader_parameter_prefix();
		overlay["texture"] = describe_texture(binding->get_texture());
		overlay["material"] = describe_material(binding->get_material());
		overlay["fallback_reason"] =
			binding->get_texture_coordinate_index() < 0
				? String("overlay_uv_set_not_realized")
				: String();
		overlayDiagnostics.push_back(overlay);
	}
	result["raster_overlays"] = overlayDiagnostics;
	return result;
}

Ref<CesiumPrimitiveFeatures>
CesiumLoadedTilePrimitive::get_primitive_features() const {
	return this->m_primitiveFeatures;
}

Ref<CesiumPrimitiveFeatures>
CesiumLoadedTilePrimitive::get_instance_features() const {
	return this->m_instanceFeatures;
}

Ref<CesiumPrimitiveMetadata>
CesiumLoadedTilePrimitive::get_primitive_metadata() const {
	return this->m_primitiveMetadata;
}

Ref<CesiumModelMetadata>
CesiumLoadedTilePrimitive::get_model_metadata() const {
	return this->m_modelMetadata;
}

int32_t CesiumLoadedTilePrimitive::get_texture_coordinate_index(
	const String& semantic
) const {
	if (!this->m_textureCoordinateMappings.has(semantic)) {
		return -1;
	}
	return this->m_textureCoordinateMappings[semantic];
}

int32_t CesiumLoadedTilePrimitive::get_overlay_texture_coordinate_index(
	int32_t textureCoordinateId
) const {
	return this->get_texture_coordinate_index(
		"_CESIUMOVERLAY_" + String::num_int64(textureCoordinateId)
	);
}

Vector3 CesiumLoadedTilePrimitive::get_absolute_origin() const {
	return this->m_absoluteOrigin;
}

Vector3 CesiumLoadedTilePrimitive::get_absolute_origin_high() const {
	return this->m_absoluteOriginHigh;
}

Vector3 CesiumLoadedTilePrimitive::get_absolute_origin_low() const {
	return this->m_absoluteOriginLow;
}

Basis CesiumLoadedTilePrimitive::get_local_to_absolute_basis() const {
	return this->m_localToAbsoluteBasis;
}

bool CesiumLoadedTilePrimitive::apply_world_coordinate_parameters(
	const Ref<Material>& material,
	const String& parameterPrefix
) const {
	Ref<ShaderMaterial> shaderMaterial = material;
	if (shaderMaterial.is_null() || parameterPrefix.is_empty()) {
		return false;
	}
	shaderMaterial->set_shader_parameter(
		parameterPrefix + String("_absolute_origin_high"),
		this->m_absoluteOriginHigh
	);
	shaderMaterial->set_shader_parameter(
		parameterPrefix + String("_absolute_origin_low"),
		this->m_absoluteOriginLow
	);
	shaderMaterial->set_shader_parameter(
		parameterPrefix + String("_local_to_absolute_basis"),
		this->m_localToAbsoluteBasis
	);
	return true;
}

void CesiumLoadedTilePrimitive::initialize(
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
) {
	this->m_tile = tile == nullptr ? ObjectID() : ObjectID(tile->get_instance_id());
	this->m_renderNode = renderNode == nullptr
		? ObjectID()
		: ObjectID(renderNode->get_instance_id());
	const CesiumGltfInstancedComponent* instancedComponent =
		Object::cast_to<CesiumGltfInstancedComponent>(renderNode);
	this->m_gpuInstanced = instancedComponent != nullptr;
	this->m_instanceCount = instancedComponent == nullptr
		? 0
		: instancedComponent->get_instance_count();
	this->m_tileId = tileId;
	this->m_tileExtras = tile == nullptr
		? Variant()
		: tile->get_tile_extras().duplicate(true);
	this->m_surfaceIndex = rendererPrimitiveIndex;
	this->m_meshSurfaceIndex = meshSurfaceIndex;
	this->m_nodeIndex = nodeIndex;
	this->m_meshIndex = meshIndex;
	this->m_primitiveIndex = primitiveIndex;
	this->m_materialIndex = materialIndex;
	this->m_primitiveMode = primitiveMode;
	this->m_pointCount = pointCount;
	this->m_pointDiameter = pointDiameter;
	this->m_primitiveDimensions = primitiveDimensions;
	this->m_tileGeometricError = tileGeometricError;
	this->m_usesAdditiveRefinement = usesAdditiveRefinement;
	this->m_attributes = attributes.duplicate(true);
	this->m_gltfMaterial = gltfMaterial.duplicate(true);
	this->m_translucent = this->m_gltfMaterial.has("alpha_mode") &&
		String(this->m_gltfMaterial["alpha_mode"]) == String("BLEND");
	this->m_translucencyIsolated = this->m_translucent &&
		renderNode != nullptr && renderNode != tile;
	this->m_textureCoordinateMappings = textureCoordinateMappings.duplicate(true);
	this->m_defaultMaterial = defaultMaterial.is_valid()
		? ObjectID(defaultMaterial->get_instance_id())
		: ObjectID();
	this->m_selectedBaseMaterial = this->m_defaultMaterial;
	this->m_primitiveFeatures = primitiveFeatures;
	this->m_instanceFeatures = instanceFeatures;
	this->m_primitiveMetadata = primitiveMetadata;
	this->m_modelMetadata = modelMetadata;
	if (tile != nullptr) {
		this->m_absoluteOrigin = CesiumMathUtils::from_glm_vec3(
			tile->get_original_position()
		);
		this->m_localToAbsoluteBasis = tile->get_original_basis();
		split_vector_for_shader(
			this->m_absoluteOrigin,
			&this->m_absoluteOriginHigh,
			&this->m_absoluteOriginLow
		);
	}
}

void CesiumLoadedTilePrimitive::set_selected_base_material(
	const Ref<Material>& material,
	bool customMaterial
) {
	this->m_selectedBaseMaterial = material.is_valid()
		? ObjectID(material->get_instance_id())
		: ObjectID();
	this->m_usesCustomMaterial = customMaterial;
}

void CesiumLoadedTilePrimitive::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_loaded_tile"), &CesiumLoadedTilePrimitive::get_loaded_tile);
	ClassDB::bind_method(D_METHOD("get_render_node"), &CesiumLoadedTilePrimitive::get_render_node);
	ClassDB::bind_method(D_METHOD("get_tile_id"), &CesiumLoadedTilePrimitive::get_tile_id);
	ClassDB::bind_method(D_METHOD("get_tile_extras"), &CesiumLoadedTilePrimitive::get_tile_extras);
	ClassDB::bind_method(D_METHOD("get_surface_index"), &CesiumLoadedTilePrimitive::get_surface_index);
	ClassDB::bind_method(D_METHOD("get_mesh_surface_index"), &CesiumLoadedTilePrimitive::get_mesh_surface_index);
	ClassDB::bind_method(D_METHOD("is_gpu_instanced"), &CesiumLoadedTilePrimitive::is_gpu_instanced);
	ClassDB::bind_method(D_METHOD("get_instance_count"), &CesiumLoadedTilePrimitive::get_instance_count);
	ClassDB::bind_method(D_METHOD("get_node_index"), &CesiumLoadedTilePrimitive::get_node_index);
	ClassDB::bind_method(D_METHOD("get_mesh_index"), &CesiumLoadedTilePrimitive::get_mesh_index);
	ClassDB::bind_method(D_METHOD("get_primitive_index"), &CesiumLoadedTilePrimitive::get_primitive_index);
	ClassDB::bind_method(D_METHOD("get_material_index"), &CesiumLoadedTilePrimitive::get_material_index);
	ClassDB::bind_method(D_METHOD("get_primitive_mode"), &CesiumLoadedTilePrimitive::get_primitive_mode);
	ClassDB::bind_method(D_METHOD("get_primitive_mode_name"), &CesiumLoadedTilePrimitive::get_primitive_mode_name);
	ClassDB::bind_method(D_METHOD("is_point_primitive"), &CesiumLoadedTilePrimitive::is_point_primitive);
	ClassDB::bind_method(D_METHOD("is_line_primitive"), &CesiumLoadedTilePrimitive::is_line_primitive);
	ClassDB::bind_method(D_METHOD("is_triangle_primitive"), &CesiumLoadedTilePrimitive::is_triangle_primitive);
	ClassDB::bind_method(D_METHOD("is_translucent"), &CesiumLoadedTilePrimitive::is_translucent);
	ClassDB::bind_method(D_METHOD("is_translucency_isolated"), &CesiumLoadedTilePrimitive::is_translucency_isolated);
	ClassDB::bind_method(D_METHOD("get_point_count"), &CesiumLoadedTilePrimitive::get_point_count);
	ClassDB::bind_method(D_METHOD("get_point_diameter"), &CesiumLoadedTilePrimitive::get_point_diameter);
	ClassDB::bind_method(D_METHOD("get_primitive_dimensions"), &CesiumLoadedTilePrimitive::get_primitive_dimensions);
	ClassDB::bind_method(D_METHOD("get_tile_geometric_error"), &CesiumLoadedTilePrimitive::get_tile_geometric_error);
	ClassDB::bind_method(D_METHOD("get_uses_additive_refinement"), &CesiumLoadedTilePrimitive::get_uses_additive_refinement);
	ClassDB::bind_method(D_METHOD("get_point_cloud_parameters"), &CesiumLoadedTilePrimitive::get_point_cloud_parameters);
	ClassDB::bind_method(D_METHOD("get_attributes"), &CesiumLoadedTilePrimitive::get_attributes);
	ClassDB::bind_method(D_METHOD("get_gltf_material"), &CesiumLoadedTilePrimitive::get_gltf_material);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_mappings"), &CesiumLoadedTilePrimitive::get_texture_coordinate_mappings);
	ClassDB::bind_method(D_METHOD("get_default_material"), &CesiumLoadedTilePrimitive::get_default_material);
	ClassDB::bind_method(D_METHOD("get_selected_base_material"), &CesiumLoadedTilePrimitive::get_selected_base_material);
	ClassDB::bind_method(D_METHOD("get_active_material"), &CesiumLoadedTilePrimitive::get_active_material);
	ClassDB::bind_method(D_METHOD("get_uses_custom_material"), &CesiumLoadedTilePrimitive::get_uses_custom_material);
	ClassDB::bind_method(D_METHOD("get_raster_overlay_bindings"), &CesiumLoadedTilePrimitive::get_raster_overlay_bindings);
	ClassDB::bind_method(D_METHOD("get_render_diagnostics"), &CesiumLoadedTilePrimitive::get_render_diagnostics);
	ClassDB::bind_method(D_METHOD("get_primitive_features"), &CesiumLoadedTilePrimitive::get_primitive_features);
	ClassDB::bind_method(D_METHOD("get_instance_features"), &CesiumLoadedTilePrimitive::get_instance_features);
	ClassDB::bind_method(D_METHOD("get_primitive_metadata"), &CesiumLoadedTilePrimitive::get_primitive_metadata);
	ClassDB::bind_method(D_METHOD("get_model_metadata"), &CesiumLoadedTilePrimitive::get_model_metadata);
	ClassDB::bind_method(D_METHOD("get_texture_coordinate_index", "semantic"), &CesiumLoadedTilePrimitive::get_texture_coordinate_index);
	ClassDB::bind_method(D_METHOD("get_overlay_texture_coordinate_index", "texture_coordinate_id"), &CesiumLoadedTilePrimitive::get_overlay_texture_coordinate_index);
	ClassDB::bind_method(D_METHOD("get_absolute_origin"), &CesiumLoadedTilePrimitive::get_absolute_origin);
	ClassDB::bind_method(D_METHOD("get_absolute_origin_high"), &CesiumLoadedTilePrimitive::get_absolute_origin_high);
	ClassDB::bind_method(D_METHOD("get_absolute_origin_low"), &CesiumLoadedTilePrimitive::get_absolute_origin_low);
	ClassDB::bind_method(D_METHOD("get_local_to_absolute_basis"), &CesiumLoadedTilePrimitive::get_local_to_absolute_basis);
	ClassDB::bind_method(
		D_METHOD("apply_world_coordinate_parameters", "material", "parameter_prefix"),
		&CesiumLoadedTilePrimitive::apply_world_coordinate_parameters,
		DEFVAL("cesium")
	);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "loaded_tile", PROPERTY_HINT_NODE_TYPE, "Cesium3DTile", PROPERTY_USAGE_NONE), "", "get_loaded_tile");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "render_node", PROPERTY_HINT_NODE_TYPE, "GeometryInstance3D", PROPERTY_USAGE_NONE), "", "get_render_node");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tile_id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_tile_id");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "tile_extras", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_tile_extras");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "surface_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_surface_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_surface_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_mesh_surface_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gpu_instanced", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_gpu_instanced");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "instance_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_instance_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "node_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_node_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_mesh_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "primitive_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_primitive_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "material_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_material_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "primitive_mode", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_primitive_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "primitive_mode_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_primitive_mode_name");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "point_primitive", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_point_primitive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "line_primitive", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_line_primitive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "triangle_primitive", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_triangle_primitive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "translucent", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_translucent");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "translucency_isolated", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "is_translucency_isolated");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "point_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_point_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "point_diameter", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_point_diameter");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "primitive_dimensions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_primitive_dimensions");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tile_geometric_error", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_tile_geometric_error");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "uses_additive_refinement", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_uses_additive_refinement");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "point_cloud_parameters", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_point_cloud_parameters");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "attributes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "gltf_material", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_gltf_material");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "texture_coordinate_mappings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_texture_coordinate_mappings");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "default_material", PROPERTY_HINT_RESOURCE_TYPE, "Material", PROPERTY_USAGE_NONE), "", "get_default_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "selected_base_material", PROPERTY_HINT_RESOURCE_TYPE, "Material", PROPERTY_USAGE_NONE), "", "get_selected_base_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "active_material", PROPERTY_HINT_RESOURCE_TYPE, "Material", PROPERTY_USAGE_NONE), "", "get_active_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "uses_custom_material", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_uses_custom_material");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "raster_overlay_bindings", PROPERTY_HINT_ARRAY_TYPE, "CesiumRasterOverlayBinding", PROPERTY_USAGE_NONE), "", "get_raster_overlay_bindings");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "render_diagnostics", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_render_diagnostics");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "primitive_features", PROPERTY_HINT_RESOURCE_TYPE, "CesiumPrimitiveFeatures", PROPERTY_USAGE_NONE), "", "get_primitive_features");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "instance_features", PROPERTY_HINT_RESOURCE_TYPE, "CesiumPrimitiveFeatures", PROPERTY_USAGE_NONE), "", "get_instance_features");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "primitive_metadata", PROPERTY_HINT_RESOURCE_TYPE, "CesiumPrimitiveMetadata", PROPERTY_USAGE_NONE), "", "get_primitive_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model_metadata", PROPERTY_HINT_RESOURCE_TYPE, "CesiumModelMetadata", PROPERTY_USAGE_NONE), "", "get_model_metadata");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "absolute_origin", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_absolute_origin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "absolute_origin_high", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_absolute_origin_high");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "absolute_origin_low", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_absolute_origin_low");
	ADD_PROPERTY(PropertyInfo(Variant::BASIS, "local_to_absolute_basis", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "", "get_local_to_absolute_basis");
}
