// Material parameter and sampler conversion in this file are adapted from
// Cesium for Unreal's CesiumGltfComponent, CesiumTextureUtility, and
// GenerateMaterialUtility implementations (Apache-2.0).
// Copyright 2020-2026 CesiumGS, Inc. and Contributors.

#include "Runtime/Private/Materials/CesiumGltfMaterialLoader.h"

#include "CesiumGltf/ExtensionKhrMaterialsUnlit.h"
#include "CesiumGltf/ExtensionKhrTextureTransform.h"
#include "CesiumGltf/Image.h"
#include "CesiumGltf/KhrTextureTransform.h"
#include "CesiumGltf/Sampler.h"
#include "CesiumGltf/Texture.h"
#include "CesiumUtility/JsonValue.h"
#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"

#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
#include "godot_cpp/variant/vector2i.hpp"

#include <algorithm>
#include <optional>
#include <sstream>

namespace {
const CesiumGltf::Material& get_default_material() {
	static const CesiumGltf::Material defaultMaterial;
	return defaultMaterial;
}

Color vector_to_color(
	const std::vector<double>& value,
	const Color& fallback
) {
	if (value.size() < 4) {
		return fallback;
	}
	// glTF factors are already linear. Textures tagged source_color are decoded
	// from sRGB by Godot, but numeric factors must not be transfer-converted.
	return Color(value[0], value[1], value[2], value[3]);
}

Color emissive_to_color(const std::vector<double>& value) {
	if (value.size() < 3) {
		return Color(0.0, 0.0, 0.0, 1.0);
	}
	return Color(value[0], value[1], value[2], 1.0);
}

bool contains_extension(
	const std::vector<std::string>& extensions,
	const std::string& extension
) {
	return std::find(extensions.begin(), extensions.end(), extension) !=
		extensions.end();
}

int32_t feature_wrap_mode(int32_t wrap) {
	if (wrap == CesiumGltf::Sampler::WrapS::CLAMP_TO_EDGE) {
		return 0;
	}
	if (wrap == CesiumGltf::Sampler::WrapS::MIRRORED_REPEAT) {
		return 2;
	}
	return 1;
}
}

CesiumGltfMaterialLoader::CesiumGltfMaterialLoader(
	const CesiumGltf::Model& model,
	const std::shared_ptr<CesiumGltfImageAssetResourceCache>& sharedImageCache,
	const std::unordered_map<
		const CesiumImage::ImageAsset*,
		CesiumGltfImageContentFingerprint
	>* imageContentFingerprints,
	const std::unordered_set<const CesiumImage::ImageAsset*>* cpuImageAssets,
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>*
		sharedImageResources,
	bool enableLodTransitionDither,
	bool enableTranslucencyDepthPrepass
) : m_model(model),
	m_sharedImageCache(sharedImageCache),
	m_imageContentFingerprints(imageContentFingerprints),
	m_cpuImageAssets(cpuImageAssets),
	m_sharedImageResources(sharedImageResources),
	m_enableLodTransitionDither(enableLodTransitionDither),
	m_enableTranslucencyDepthPrepass(enableTranslucencyDepthPrepass) {}

CesiumGltfMaterialLoader::SamplerState
CesiumGltfMaterialLoader::get_sampler_state(int32_t textureIndex) const {
	SamplerState result;
	if (
		textureIndex < 0 ||
		textureIndex >= static_cast<int32_t>(this->m_model.textures.size())
	) {
		return result;
	}

	const CesiumGltf::Texture& texture = this->m_model.textures[textureIndex];
	CesiumGltf::Sampler defaultSampler;
	const CesiumGltf::Sampler* sampler = &defaultSampler;
	if (
		texture.sampler >= 0 &&
		texture.sampler < static_cast<int32_t>(this->m_model.samplers.size())
	) {
		sampler = &this->m_model.samplers[texture.sampler];
	}

	const int32_t minFilter = sampler->minFilter.value_or(
		CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_LINEAR
	);
	switch (minFilter) {
	case CesiumGltf::Sampler::MinFilter::NEAREST:
		result.shaderFilterHint = "filter_nearest";
		break;
	case CesiumGltf::Sampler::MinFilter::LINEAR:
		result.shaderFilterHint = "filter_linear";
		break;
	case CesiumGltf::Sampler::MinFilter::NEAREST_MIPMAP_NEAREST:
	case CesiumGltf::Sampler::MinFilter::NEAREST_MIPMAP_LINEAR:
		result.shaderFilterHint = "filter_nearest_mipmap";
		// Godot exposes one nearest-mipmap shader hint, so it cannot preserve
		// the distinction between nearest and linear mip-level interpolation.
		result.exactFilter =
			minFilter == CesiumGltf::Sampler::MinFilter::NEAREST_MIPMAP_NEAREST;
		break;
	case CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_NEAREST:
	case CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_LINEAR:
	default:
		result.shaderFilterHint = "filter_linear_mipmap";
		result.exactFilter =
			minFilter == CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_LINEAR;
		break;
	}

	if (sampler->magFilter) {
		const bool magNearest =
			sampler->magFilter.value() == CesiumGltf::Sampler::MagFilter::NEAREST;
		const bool shaderNearest =
			result.shaderFilterHint.find("nearest") != std::string::npos;
		if (magNearest != shaderNearest) {
			// ShaderMaterial sampler hints do not expose independent minification
			// and magnification filters.
			result.exactFilter = false;
		}
	}

	auto convertWrap = [](int32_t wrap) -> WrapMode {
		if (wrap == CesiumGltf::Sampler::WrapS::CLAMP_TO_EDGE) {
			return WrapMode::Clamp;
		}
		if (wrap == CesiumGltf::Sampler::WrapS::MIRRORED_REPEAT) {
			return WrapMode::MirroredRepeat;
		}
		return WrapMode::Repeat;
	};
	result.wrapS = convertWrap(sampler->wrapS);
	result.wrapT = convertWrap(sampler->wrapT);
	return result;
}

Ref<ImageTexture> CesiumGltfMaterialLoader::load_texture(
	int32_t textureIndex,
	Error* error
) {
	ERR_FAIL_NULL_V(error, Ref<ImageTexture>());
	if (
		textureIndex < 0 ||
		textureIndex >= static_cast<int32_t>(this->m_model.textures.size())
	) {
		*error = Error::ERR_INVALID_DATA;
		return Ref<ImageTexture>();
	}
	const CesiumGltf::Texture& texture = this->m_model.textures[textureIndex];
	if (
		texture.source < 0 ||
		texture.source >= static_cast<int32_t>(this->m_model.images.size())
	) {
		*error = Error::ERR_INVALID_DATA;
		return Ref<ImageTexture>();
	}
	const CesiumGltf::Image& image = this->m_model.images[texture.source];
	if (image.pAsset == nullptr) {
		*error = Error::ERR_INVALID_DATA;
		return Ref<ImageTexture>();
	}

	const CesiumImage::ImageAsset* imageAsset = image.pAsset.get();
	auto existing = this->m_textureCache.find(imageAsset);
	if (existing != this->m_textureCache.end()) {
		return existing->second;
	}
	Ref<ImageTexture> loaded;
	if (this->m_sharedImageCache != nullptr) {
		CesiumGltfImageContentFingerprint fingerprint;
		bool hasPreparedFingerprint = false;
		if (this->m_imageContentFingerprints != nullptr) {
			auto prepared =
				this->m_imageContentFingerprints->find(imageAsset);
			if (prepared != this->m_imageContentFingerprints->end()) {
				fingerprint = prepared->second;
				hasPreparedFingerprint = true;
			}
		}
		if (!hasPreparedFingerprint) {
			fingerprint =
				CesiumGltfImageAssetResourceCache::fingerprint(*imageAsset);
		}
		std::shared_ptr<CesiumGltfSharedImageResource> resource =
			this->m_sharedImageCache->acquire(
				image.pAsset,
				fingerprint,
				this->m_cpuImageAssets != nullptr &&
					this->m_cpuImageAssets->contains(imageAsset),
				error
			);
		if (resource != nullptr) {
			loaded = resource->texture;
			if (this->m_sharedImageResources != nullptr) {
				this->m_sharedImageResources->emplace_back(std::move(resource));
			}
		}
	} else {
		loaded = CesiumGDTextureLoader::load_image_texture(*imageAsset, true);
	}
	if (loaded.is_null()) {
		if (*error == Error::OK) {
			*error = Error::ERR_CANT_CREATE;
		}
		return loaded;
	}
	this->m_textureCache.emplace(imageAsset, loaded);
	return loaded;
}

CesiumGltfMaterialLoader::TextureSlot
CesiumGltfMaterialLoader::load_texture_slot(
	const CesiumGltf::TextureInfo* textureInfo,
	Error* error
) {
	TextureSlot result;
	if (textureInfo == nullptr) {
		return result;
	}
	result.texture = this->load_texture(textureInfo->index, error);
	if (result.texture.is_null()) {
		return result;
	}
	result.present = true;
	result.sampler = this->get_sampler_state(textureInfo->index);
	result.textureCoordinateSet = textureInfo->texCoord;

	const CesiumGltf::ExtensionKhrTextureTransform* transform =
		textureInfo->getExtension<CesiumGltf::ExtensionKhrTextureTransform>();
	if (transform != nullptr) {
		CesiumGltf::KhrTextureTransform validated(*transform);
		if (
			validated.status() !=
			CesiumGltf::KhrTextureTransformStatus::Valid
		) {
			*error = Error::ERR_INVALID_DATA;
			return TextureSlot();
		}
		result.offset = Vector2(transform->offset[0], transform->offset[1]);
		result.scale = Vector2(transform->scale[0], transform->scale[1]);
		result.rotation = transform->rotation;
		result.textureCoordinateSet = transform->texCoord.value_or(
			textureInfo->texCoord
		);
	}
	if (
		result.textureCoordinateSet < 0 ||
		result.textureCoordinateSet > 1
	) {
		// Godot ArrayMesh exposes UV and UV2. Silently sampling another set as
		// UV0 is worse than omitting the texture and reporting malformed/unsupported
		// content to the caller.
		*error = Error::ERR_UNAVAILABLE;
		return TextureSlot();
	}
	return result;
}

std::string CesiumGltfMaterialLoader::build_shader_code(
	const CesiumGltf::Material& material,
	const TextureSlot& baseColor,
	const TextureSlot& metallicRoughness,
	const TextureSlot& normal,
	const TextureSlot& occlusion,
	const TextureSlot& emissive,
	const CesiumFeatureStyleEncoding* featureStyle,
	const CesiumGltfPrimitiveMaterialOptions& primitiveOptions
) {
	std::ostringstream code;
	code << "shader_type spatial;\nrender_mode blend_mix";
	if (material.doubleSided) {
		code << ", cull_disabled";
	} else {
		// This renderer retains the established glTF-to-Godot winding mapping.
		code << ", cull_front";
	}
	if (
		material.hasExtension<CesiumGltf::ExtensionKhrMaterialsUnlit>() ||
		primitiveOptions.forceUnshaded
	) {
		code << ", unshaded";
	}
	if (material.alphaMode == CesiumGltf::Material::AlphaMode::MASK) {
		code << ", depth_prepass_alpha, alpha_to_coverage";
	} else if (
		material.alphaMode == CesiumGltf::Material::AlphaMode::BLEND &&
		this->m_enableTranslucencyDepthPrepass
	) {
		// Preserve glTF blending for partial-alpha fragments while allowing
		// fully opaque texels to populate depth first. This improves foliage and
		// mostly-opaque blended assets without converting them to alpha scissor.
		code << ", depth_prepass_alpha";
	}
	code << ";\n\n";
	code << "uniform vec4 base_color_factor = vec4(1.0);\n";
	code << "uniform float metallic_factor = 1.0;\n";
	code << "uniform float roughness_factor = 1.0;\n";
	code << "uniform vec3 emissive_factor = vec3(0.0);\n";
	code << "uniform float normal_scale = 1.0;\n";
	code << "uniform float occlusion_strength = 1.0;\n";
	code << "uniform float alpha_cutoff = 0.5;\n";
	code << "uniform bool double_sided = false;\n";
	// `CesiumPolygonRasterOverlay` reserves the stable `clipping` material key.
	// Declaring the contract on generated materials makes polygon clipping work
	// without an application-specific shader while leaving all ordinary content
	// on the exact same shader path when no mask is attached.
	code << "uniform bool clipping_enabled = false;\n";
	code << "uniform sampler2D clipping_texture : "
			"filter_linear_mipmap, repeat_disable;\n";
	code << "uniform vec4 clipping_translation_scale = "
			"vec4(0.0, 0.0, 1.0, 1.0);\n";
	code << "uniform int clipping_texture_coordinate_index = -1;\n";
	// Mirror Cesium for Unreal's default Overlay0 material layer. Provider nodes
	// retain arbitrary keys for application shaders, while this one standard key
	// makes the default generated glTF material useful for imagery and the tile
	// color diagnostic without an application-specific material receiver.
	code << "uniform sampler2D overlay0_texture : "
			"source_color, filter_linear_mipmap, repeat_disable;\n";
	code << "uniform vec4 overlay0_translation_scale = "
			"vec4(0.0, 0.0, 1.0, 1.0);\n";
	code << "uniform int overlay0_texture_coordinate_index = -1;\n";
	if (primitiveOptions.pointPrimitive) {
		// Godot exposes point size, point coordinates, projection, and viewport
		// dimensions directly to spatial shaders. This is the renderer-native
		// counterpart of Cesium for Unreal's point attenuation vertex factory:
		// no CPU quad expansion and no per-frame material updates are required.
		code << "uniform bool cesium_point_attenuation_enabled = false;\n";
		code << "uniform float cesium_point_geometric_error = 0.0;\n";
		code << "uniform float cesium_point_maximum_size = 1.0;\n";
		code << "uniform float cesium_point_diameter = 0.0;\n";
	}
	if (this->m_enableLodTransitionDither) {
		// The controller shallow-duplicates only the ShaderMaterial parameter
		// container per tile placement. Meshes, shaders, and textures remain
		// shared, while regular uniforms also support Godot 4.6 Compatibility.
		// Screen-space stochastic discard keeps opaque depth/shadow behavior and
		// avoids Godot's slower, sorting-sensitive transparent pipeline.
		code << "uniform float cesium_lod_transition_percentage "
				": hint_range(0.0, 1.0) = 1.0;\n";
		code << "uniform bool cesium_lod_fading_out = false;\n";
	}
	const bool hasFeatureStyle = featureStyle != nullptr &&
		featureStyle->valid;
	if (hasFeatureStyle) {
		code << "uniform bool cesium_feature_style_enabled = false;\n";
		code << "uniform sampler2D cesium_feature_style_colors : "
				"filter_nearest, repeat_disable;\n";
		code << "uniform sampler2D cesium_feature_style_controls : "
				"filter_nearest, repeat_disable;\n";
		code << "uniform int cesium_feature_style_dimension = 1;\n";
		code << "uniform float cesium_feature_style_feature_count = 0.0;\n";
		code << "uniform float cesium_feature_style_null_id = -1.0;\n";
		if (featureStyle->source == CesiumFeatureStyleSource::Texture) {
			code << "uniform sampler2D cesium_feature_id_texture : "
					"filter_nearest, repeat_disable;\n";
			code << "uniform int cesium_feature_id_uv_set = 0;\n";
			code << "uniform vec4 cesium_feature_id_texture_transform = "
					"vec4(0.0, 0.0, 1.0, 1.0);\n";
			code << "uniform float cesium_feature_id_texture_rotation = 0.0;\n";
			code << "uniform ivec2 cesium_feature_id_texture_wrap = "
					"ivec2(1, 1);\n";
			code << "uniform vec4 cesium_feature_id_channel_weights = "
					"vec4(0.0);\n";
		} else {
			code << "varying flat float cesium_feature_id;\n";
		}
	}

	auto declareSlot = [&code](
		const char* prefix,
		const TextureSlot& slot,
		bool sourceColor,
		bool normalMap
	) {
		code << "uniform bool has_" << prefix << "_texture = false;\n";
		code << "uniform sampler2D " << prefix << "_texture : ";
		if (sourceColor) {
			code << "source_color, ";
		}
		if (normalMap) {
			code << "hint_normal, ";
		}
		code << slot.sampler.shaderFilterHint << ", repeat_disable;\n";
		code << "uniform int " << prefix << "_uv_set = 0;\n";
		code << "uniform vec4 " << prefix
			 << "_texture_transform = vec4(0.0, 0.0, 1.0, 1.0);\n";
		code << "uniform float " << prefix << "_texture_rotation = 0.0;\n";
		code << "uniform ivec2 " << prefix
			 << "_texture_wrap = ivec2(1, 1);\n";
	};
	declareSlot("base_color", baseColor, true, false);
	declareSlot("metallic_roughness", metallicRoughness, false, false);
	declareSlot("normal", normal, false, true);
	declareSlot("occlusion", occlusion, false, false);
	declareSlot("emissive", emissive, true, false);
	const bool hasVertexFeatureStyle = hasFeatureStyle &&
		featureStyle->source != CesiumFeatureStyleSource::Texture;
	if (primitiveOptions.pointPrimitive || hasVertexFeatureStyle) {
		code << "\nvoid vertex() {\n";
		if (primitiveOptions.pointPrimitive) {
			code << R"(
	if (cesium_point_diameter >= 1.0) {
		POINT_SIZE = cesium_point_diameter;
	} else if (cesium_point_attenuation_enabled) {
		vec4 cesium_point_view_position =
			MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
		float cesium_point_depth = max(
			abs(cesium_point_view_position.z),
			0.000001
		);
		float cesium_point_depth_multiplier =
			VIEWPORT_SIZE.y * abs(PROJECTION_MATRIX[1][1]) * 0.5;
		POINT_SIZE = min(
			(cesium_point_geometric_error / cesium_point_depth) *
				cesium_point_depth_multiplier,
			cesium_point_maximum_size
		);
	} else {
		POINT_SIZE = 1.0;
	}
)";
		}
		if (
			hasVertexFeatureStyle &&
			featureStyle->source == CesiumFeatureStyleSource::Instance
		) {
			code << "\tcesium_feature_id = INSTANCE_CUSTOM.z > 0.5 "
					"? INSTANCE_CUSTOM.x + INSTANCE_CUSTOM.y * 65536.0 "
					": -1.0;\n";
		} else if (hasVertexFeatureStyle) {
			code << "\tcesium_feature_id = CUSTOM0.z > 0.5 "
					"? CUSTOM0.x + CUSTOM0.y * 65536.0 "
					": -1.0;\n";
		}
		code << "}\n";
	}
	if (this->m_enableLodTransitionDither) {
		code << R"(
float cesium_lod_dither(vec2 fragment_coordinate) {
	return fract(
		52.9829189 * fract(
			dot(fragment_coordinate, vec2(0.06711056, 0.00583715))
		)
	);
}

bool cesium_lod_should_discard(vec2 fragment_coordinate) {
	float threshold = clamp(cesium_lod_transition_percentage, 0.0, 1.0);
	float dither = cesium_lod_dither(fragment_coordinate);
	// Incoming and outgoing LODs use complementary halves of the exact same
	// screen-space pattern. When their coverage overlaps, every pixel belongs
	// to one of them rather than both versions discarding the same pixels.
	return cesium_lod_fading_out
		? dither < threshold
		: dither >= threshold;
}

)";
	}

	code << R"(
float wrap_coordinate(float value, int mode) {
	if (mode == 0) {
		return clamp(value, 0.0, 1.0);
	}
	if (mode == 2) {
		return 1.0 - abs(mod(value, 2.0) - 1.0);
	}
	return fract(value);
}

vec2 texture_coordinate(
	vec2 uv0,
	vec2 uv1,
	int uv_set,
	vec4 texture_transform,
	float rotation,
	ivec2 wrap_mode
) {
	vec2 source_uv = uv_set == 1 ? uv1 : uv0;
	vec2 scaled = source_uv * texture_transform.zw;
	float sine = sin(rotation);
	float cosine = cos(rotation);
	vec2 rotated = vec2(
		scaled.x * cosine + scaled.y * sine,
		scaled.y * cosine - scaled.x * sine
	);
	vec2 transformed = rotated + texture_transform.xy;
	return vec2(
		wrap_coordinate(transformed.x, wrap_mode.x),
		wrap_coordinate(transformed.y, wrap_mode.y)
	);
}

void fragment() {
)";
	if (primitiveOptions.pointPrimitive) {
		// BENTLEY_materials_point_style defines a circular point diameter. Plain
		// Cesium attenuation intentionally remains square, matching upstream.
		code << R"(
	if (
		cesium_point_diameter >= 1.0 &&
		distance(POINT_COORD, vec2(0.5)) > 0.5
	) {
		discard;
	}
)";
	}
	if (this->m_enableLodTransitionDither) {
		// Godot 4.6 allows discard directly in fragment(), not through a helper.
		code << "\tif (cesium_lod_should_discard(FRAGCOORD.xy)) {\n"
				"\t\tdiscard;\n"
				"\t}\n";
	}
	code << R"(
	if (clipping_enabled && clipping_texture_coordinate_index >= 0) {
		vec2 clipping_source_uv = clipping_texture_coordinate_index == 1
			? UV2
			: UV;
		vec2 clipping_uv = clipping_source_uv *
			clipping_translation_scale.zw +
			clipping_translation_scale.xy;
		// Cesium raster coordinates use a bottom-left image origin while Godot
		// samples image rows from the top-left.
		clipping_uv.y = 1.0 - clipping_uv.y;
		if (texture(clipping_texture, clipping_uv).r > 0.5) {
			discard;
		}
	}
)";
	code << R"(
	if (double_sided && !FRONT_FACING) {
		NORMAL = -NORMAL;
	}
	vec4 base_color = base_color_factor * COLOR;
	if (has_base_color_texture) {
		base_color *= texture(
			base_color_texture,
			texture_coordinate(
				UV,
				UV2,
				base_color_uv_set,
				base_color_texture_transform,
				base_color_texture_rotation,
				base_color_texture_wrap
			)
		);
	}
)";
	if (hasFeatureStyle) {
		if (featureStyle->source == CesiumFeatureStyleSource::Texture) {
			code << R"(
	vec4 cesium_feature_id_bytes = floor(
		texture(
			cesium_feature_id_texture,
			texture_coordinate(
				UV,
				UV2,
				cesium_feature_id_uv_set,
				cesium_feature_id_texture_transform,
				cesium_feature_id_texture_rotation,
				cesium_feature_id_texture_wrap
			)
		) * 255.0 + 0.5
	);
	float cesium_resolved_feature_id = dot(
		cesium_feature_id_bytes,
		cesium_feature_id_channel_weights
	);
)";
		} else {
			code << "\tfloat cesium_resolved_feature_id = cesium_feature_id;\n";
		}
		code << R"(
	if (
		cesium_feature_style_enabled &&
		cesium_resolved_feature_id >= 0.0 &&
		cesium_resolved_feature_id < cesium_feature_style_feature_count &&
		abs(
			cesium_resolved_feature_id - cesium_feature_style_null_id
		) > 0.25
	) {
		int cesium_style_index = int(floor(cesium_resolved_feature_id + 0.5));
		ivec2 cesium_style_coordinate = ivec2(
			cesium_style_index % cesium_feature_style_dimension,
			cesium_style_index / cesium_feature_style_dimension
		);
		vec2 cesium_style_control = texelFetch(
			cesium_feature_style_controls,
			cesium_style_coordinate,
			0
		).rg;
		if (cesium_style_control.r < 0.5) {
			discard;
		}
		vec4 cesium_style_color = texelFetch(
			cesium_feature_style_colors,
			cesium_style_coordinate,
			0
		);
		base_color.rgb = mix(
			base_color.rgb,
			cesium_style_color.rgb,
			cesium_style_control.g
		);
		base_color.a *= mix(
			1.0,
			cesium_style_color.a,
			cesium_style_control.g
		);
	}
)";
	}
	code << R"(
	if (overlay0_texture_coordinate_index >= 0) {
		vec2 overlay0_source_uv = overlay0_texture_coordinate_index == 1
			? UV2
			: UV;
		vec2 overlay0_uv = overlay0_source_uv *
			overlay0_translation_scale.zw +
			overlay0_translation_scale.xy;
		// Cesium raster coordinates use a bottom-left image origin while Godot
		// samples image rows from the top-left.
		overlay0_uv.y = 1.0 - overlay0_uv.y;
		vec4 overlay0_color = texture(overlay0_texture, overlay0_uv);
		// Raster alpha describes coverage over the glTF base color. It does not
		// turn otherwise opaque geometry into a translucent mesh.
		base_color.rgb = mix(
			base_color.rgb,
			overlay0_color.rgb,
			overlay0_color.a
		);
	}
)";
	code << R"(
	ALBEDO = base_color.rgb;

	float metallic = metallic_factor;
	float roughness = roughness_factor;
	if (has_metallic_roughness_texture) {
		vec4 sample_value = texture(
			metallic_roughness_texture,
			texture_coordinate(
				UV,
				UV2,
				metallic_roughness_uv_set,
				metallic_roughness_texture_transform,
				metallic_roughness_texture_rotation,
				metallic_roughness_texture_wrap
			)
		);
		metallic *= sample_value.b;
		roughness *= sample_value.g;
	}
	METALLIC = metallic;
	ROUGHNESS = roughness;

	if (has_normal_texture) {
		NORMAL_MAP = texture(
			normal_texture,
			texture_coordinate(
				UV,
				UV2,
				normal_uv_set,
				normal_texture_transform,
				normal_texture_rotation,
				normal_texture_wrap
			)
		).rgb;
		NORMAL_MAP_DEPTH = normal_scale;
	}
	if (has_occlusion_texture) {
		float sampled_ao = texture(
			occlusion_texture,
			texture_coordinate(
				UV,
				UV2,
				occlusion_uv_set,
				occlusion_texture_transform,
				occlusion_texture_rotation,
				occlusion_texture_wrap
			)
		).r;
		AO = mix(1.0, sampled_ao, occlusion_strength);
	}

	vec3 emissive = emissive_factor;
	if (has_emissive_texture) {
		emissive *= texture(
			emissive_texture,
			texture_coordinate(
				UV,
				UV2,
				emissive_uv_set,
				emissive_texture_transform,
				emissive_texture_rotation,
				emissive_texture_wrap
			)
		).rgb;
	}
	EMISSION = emissive;
)";
	if (material.alphaMode == CesiumGltf::Material::AlphaMode::MASK) {
		code << "\tALPHA = base_color.a;\n";
		code << "\tALPHA_SCISSOR_THRESHOLD = alpha_cutoff;\n";
	} else if (material.alphaMode == CesiumGltf::Material::AlphaMode::BLEND) {
		code << "\tALPHA = base_color.a;\n";
	}
	code << "}\n";
	return code.str();
}

Ref<Shader> CesiumGltfMaterialLoader::get_or_create_shader(
	const CesiumGltf::Material& material,
	const TextureSlot& baseColor,
	const TextureSlot& metallicRoughness,
	const TextureSlot& normal,
	const TextureSlot& occlusion,
	const TextureSlot& emissive,
	const CesiumFeatureStyleEncoding* featureStyle,
	const CesiumGltfPrimitiveMaterialOptions& primitiveOptions
) {
	const std::string code = build_shader_code(
		material,
		baseColor,
		metallicRoughness,
		normal,
		occlusion,
		emissive,
		featureStyle,
		primitiveOptions
	);
	auto existing = this->m_shaderCache.find(code);
	if (existing != this->m_shaderCache.end()) {
		return existing->second;
	}
	const String godotCode = String::utf8(code.c_str());
	Ref<Shader> shader = this->m_sharedImageCache != nullptr
		? this->m_sharedImageCache->acquire_shader(godotCode)
		: Ref<Shader>();
	if (shader.is_null()) {
		shader.instantiate();
		shader->set_code(godotCode);
	}
	this->m_shaderCache.emplace(code, shader);
	return shader;
}

void CesiumGltfMaterialLoader::configure_texture_slot(
	const Ref<ShaderMaterial>& material,
	const char* prefix,
	const TextureSlot& slot
) {
	const String name(prefix);
	material->set_shader_parameter(
		String("has_") + name + String("_texture"),
		slot.present
	);
	if (!slot.present) {
		return;
	}
	material->set_shader_parameter(name + String("_texture"), slot.texture);
	material->set_shader_parameter(
		name + String("_uv_set"),
		slot.textureCoordinateSet
	);
	material->set_shader_parameter(
		name + String("_texture_transform"),
		Vector4(slot.offset.x, slot.offset.y, slot.scale.x, slot.scale.y)
	);
	material->set_shader_parameter(
		name + String("_texture_rotation"),
		slot.rotation
	);
	material->set_shader_parameter(
		name + String("_texture_wrap"),
		Vector2i(
			static_cast<int32_t>(slot.sampler.wrapS),
			static_cast<int32_t>(slot.sampler.wrapT)
		)
	);
}

Ref<Material> CesiumGltfMaterialLoader::create_material(
	const CesiumGltf::Material* sourceMaterial,
	const CesiumFeatureStyleEncoding* featureStyle,
	const CesiumGltfPrimitiveMaterialOptions& primitiveOptions,
	Error* error
) {
	ERR_FAIL_NULL_V(error, Ref<Material>());
	*error = Error::OK;
	const CesiumGltf::Material& material =
		sourceMaterial == nullptr ? get_default_material() : *sourceMaterial;
	const CesiumGltf::MaterialPBRMetallicRoughness defaultPbr;
	const CesiumGltf::MaterialPBRMetallicRoughness& pbr =
		material.pbrMetallicRoughness
		? *material.pbrMetallicRoughness
		: defaultPbr;

	auto loadSlot = [this, error](const CesiumGltf::TextureInfo* info) {
		Error slotError = Error::OK;
		TextureSlot slot = this->load_texture_slot(info, &slotError);
		if (slotError != Error::OK && *error == Error::OK) {
			*error = slotError;
		}
		return slot;
	};
	const TextureSlot baseColor = loadSlot(
		pbr.baseColorTexture ? &*pbr.baseColorTexture : nullptr
	);
	const TextureSlot metallicRoughness = loadSlot(
		pbr.metallicRoughnessTexture ? &*pbr.metallicRoughnessTexture : nullptr
	);
	const TextureSlot normal = loadSlot(
		material.normalTexture ? &*material.normalTexture : nullptr
	);
	const TextureSlot occlusion = loadSlot(
		material.occlusionTexture ? &*material.occlusionTexture : nullptr
	);
	const TextureSlot emissive = loadSlot(
		material.emissiveTexture ? &*material.emissiveTexture : nullptr
	);

	Ref<ShaderMaterial> result;
	result.instantiate();
	result->set_name(material.name.c_str());
	result->set_meta(
		"cesium_material_source",
		sourceMaterial == nullptr ? "default_missing_material" : "gltf"
	);
	result->set_meta("cesium_source_material_defined", sourceMaterial != nullptr);
	result->set_meta(
		"cesium_source_material_name",
		String(material.name.c_str())
	);
	result->set_meta(
		"cesium_lod_transition_supported",
		this->m_enableLodTransitionDither
	);
	result->set_meta("cesium_default_raster_overlay_key", "Overlay0");
	const bool sourceTranslucent =
		material.alphaMode == CesiumGltf::Material::AlphaMode::BLEND;
	result->set_meta(
		"cesium_alpha_mode",
		sourceTranslucent
			? "BLEND"
			: material.alphaMode == CesiumGltf::Material::AlphaMode::MASK
				? "MASK"
				: "OPAQUE"
	);
	result->set_meta("cesium_source_translucent", sourceTranslucent);
	result->set_meta(
		"cesium_translucency_depth_prepass",
		sourceTranslucent && this->m_enableTranslucencyDepthPrepass
	);
	result->set_meta(
		"cesium_primitive_renderer",
		primitiveOptions.pointPrimitive ? "points" : "mesh"
	);
	result->set_meta(
		"cesium_point_cloud_supported",
		primitiveOptions.pointPrimitive
	);
	result->set_shader(this->get_or_create_shader(
		material,
		baseColor,
		metallicRoughness,
		normal,
		occlusion,
		emissive,
		featureStyle,
		primitiveOptions
	));
	result->set_shader_parameter(
		"base_color_factor",
		vector_to_color(pbr.baseColorFactor, Color(1, 1, 1, 1))
	);
	result->set_shader_parameter("metallic_factor", pbr.metallicFactor);
	result->set_shader_parameter("roughness_factor", pbr.roughnessFactor);
	result->set_shader_parameter(
		"emissive_factor",
		emissive_to_color(material.emissiveFactor)
	);
	result->set_shader_parameter("alpha_cutoff", material.alphaCutoff);
	result->set_shader_parameter("double_sided", material.doubleSided);
	result->set_shader_parameter(
		"normal_scale",
		material.normalTexture ? material.normalTexture->scale : 1.0
	);
	result->set_shader_parameter(
		"occlusion_strength",
		material.occlusionTexture ? material.occlusionTexture->strength : 1.0
	);
	configure_texture_slot(result, "base_color", baseColor);
	configure_texture_slot(result, "metallic_roughness", metallicRoughness);
	configure_texture_slot(result, "normal", normal);
	configure_texture_slot(result, "occlusion", occlusion);
	configure_texture_slot(result, "emissive", emissive);

	// Applications may need source textures that are not representable by a
	// core glTF material slot (for example, a legacy layered material graph).
	// Keep this generic: exporters opt in by listing model texture indices in
	// material extras, and lifecycle receivers retrieve the realized Texture2D
	// resources from the default material metadata without decoding images a
	// second time.
	Dictionary applicationTextures;
	auto applicationTextureIndices = material.extras.find(
		"cesium_godot_application_texture_indices"
	);
	if (
		applicationTextureIndices != material.extras.end() &&
		applicationTextureIndices->second.isArray()
	) {
		for (
			const CesiumUtility::JsonValue& value :
			applicationTextureIndices->second.getArray()
		) {
			const std::optional<int32_t> textureIndex =
				value.getSafeNumber<int32_t>();
			if (
				!textureIndex || *textureIndex < 0 ||
				*textureIndex >= static_cast<int32_t>(this->m_model.textures.size()) ||
				applicationTextures.has(*textureIndex)
			) {
				continue;
			}
			Error textureError = Error::OK;
			Ref<ImageTexture> texture = this->load_texture(
				*textureIndex,
				&textureError
			);
			if (textureError != Error::OK && *error == Error::OK) {
				*error = textureError;
			}
			if (texture.is_valid()) {
				applicationTextures[*textureIndex] = texture;
			}
		}
	}
	result->set_meta("cesium_godot_application_textures", applicationTextures);

	const bool featureStyleRequested =
		featureStyle != nullptr && featureStyle->requested;
	bool featureStyleEncoded = featureStyle != nullptr && featureStyle->valid;
	result->set_meta(
		"cesium_feature_style_requested",
		featureStyleRequested
	);
	result->set_meta(
		"cesium_feature_style_encoded",
		featureStyleEncoded
	);
	if (featureStyle != nullptr) {
		result->set_meta(
			"cesium_feature_style_source",
			featureStyle->source == CesiumFeatureStyleSource::Vertex
				? "vertex"
				: featureStyle->source == CesiumFeatureStyleSource::Texture
					? "texture"
					: featureStyle->source == CesiumFeatureStyleSource::Instance
						? "instance"
						: "none"
		);
		result->set_meta(
			"cesium_feature_style_feature_id_set_index",
			featureStyle->featureIdSetIndex
		);
		result->set_meta(
			"cesium_feature_style_feature_id_set_name",
			String(featureStyle->name.c_str())
		);
		result->set_meta(
			"cesium_feature_style_feature_count",
			featureStyle->featureCount
		);
		result->set_meta(
			"cesium_feature_style_null_id",
			featureStyle->nullFeatureId
		);
		result->set_meta(
			"cesium_feature_style_property_table_index",
			featureStyle->propertyTableIndex
		);
		result->set_meta(
			"cesium_feature_style_encoding_diagnostic",
			String(featureStyle->diagnostic.c_str())
		);
	}
	if (featureStyleEncoded) {
		result->set_shader_parameter(
			"cesium_feature_style_feature_count",
			static_cast<double>(featureStyle->featureCount)
		);
		result->set_shader_parameter(
			"cesium_feature_style_null_id",
			static_cast<double>(featureStyle->nullFeatureId)
		);
		if (featureStyle->source == CesiumFeatureStyleSource::Texture) {
			const CesiumFeatureIdSetSnapshot& set = *featureStyle->featureIdSet;
			Error featureTextureError = Error::OK;
			Ref<ImageTexture> featureTexture = this->load_texture(
				set.textureIndex,
				&featureTextureError
			);
			if (featureTexture.is_null()) {
				featureStyleEncoded = false;
				result->set_meta("cesium_feature_style_encoded", false);
				result->set_meta(
					"cesium_feature_style_encoding_diagnostic",
					"feature ID texture could not be uploaded"
				);
				if (*error == Error::OK) {
					*error = featureTextureError == Error::OK
						? Error::ERR_CANT_CREATE
						: featureTextureError;
				}
			} else {
				result->set_shader_parameter(
					"cesium_feature_id_texture",
					featureTexture
				);
				result->set_shader_parameter(
					"cesium_feature_id_uv_set",
					set.textureCoordinateSetIndex
				);
				Vector4 weights;
				double weight = 1.0;
				for (int64_t channel : set.textureChannels) {
					if (channel >= 0 && channel < 4) {
						weights[static_cast<int32_t>(channel)] = weight;
					}
					weight *= 256.0;
				}
				result->set_shader_parameter(
					"cesium_feature_id_channel_weights",
					weights
				);
				Vector2 offset;
				Vector2 scale(1.0, 1.0);
				double rotation = 0.0;
				if (set.textureTransform) {
					const glm::dvec2 sourceOffset = set.textureTransform->offset();
					const glm::dvec2 sourceScale = set.textureTransform->scale();
					offset = Vector2(sourceOffset.x, sourceOffset.y);
					scale = Vector2(sourceScale.x, sourceScale.y);
					rotation = set.textureTransform->rotation();
				}
				result->set_shader_parameter(
					"cesium_feature_id_texture_transform",
					Vector4(offset.x, offset.y, scale.x, scale.y)
				);
				result->set_shader_parameter(
					"cesium_feature_id_texture_rotation",
					rotation
				);
				const CesiumGltf::Sampler* sampler = set.textureSampler.get();
				result->set_shader_parameter(
					"cesium_feature_id_texture_wrap",
					sampler == nullptr
						? Vector2i(1, 1)
						: Vector2i(
							feature_wrap_mode(sampler->wrapS),
							feature_wrap_mode(sampler->wrapT)
						)
				);
			}
		}
	}

	const bool exactSampler =
		baseColor.sampler.exactFilter &&
		metallicRoughness.sampler.exactFilter &&
		normal.sampler.exactFilter &&
		occlusion.sampler.exactFilter &&
		emissive.sampler.exactFilter;
	result->set_meta("cesium_sampler_exact", exactSampler);

	PackedStringArray unsupported;
	for (const auto& [extensionName, extensionValue] : material.extensions) {
		(void)extensionValue;
		if (
			primitiveOptions.pointPrimitive &&
			extensionName == "BENTLEY_materials_point_style"
		) {
			continue;
		}
		if (
			extensionName !=
			CesiumGltf::ExtensionKhrMaterialsUnlit::ExtensionName
		) {
			unsupported.push_back(extensionName.c_str());
			if (
				contains_extension(
					this->m_model.extensionsRequired,
					extensionName
				) &&
				*error == Error::OK
			) {
				*error = Error::ERR_UNAVAILABLE;
			}
		}
	}
	result->set_meta("cesium_unsupported_material_extensions", unsupported);
	String fallbackReason;
	if (sourceMaterial == nullptr) {
		fallbackReason = "missing_gltf_material";
	}
	if (*error != Error::OK) {
		if (!fallbackReason.is_empty()) {
			fallbackReason += ";";
		}
		fallbackReason += "invalid_texture_or_unsupported_required_extension";
	}
	result->set_meta("cesium_material_fallback_reason", fallbackReason);
	return result;
}
