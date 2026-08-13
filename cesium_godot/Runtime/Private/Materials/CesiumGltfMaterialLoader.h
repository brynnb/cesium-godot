#ifndef CESIUM_GLTF_MATERIAL_LOADER_H
#define CESIUM_GLTF_MATERIAL_LOADER_H

#include "CesiumGltf/Material.h"
#include "CesiumGltf/Model.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Runtime/Private/Metadata/CesiumFeatureStyleEncoding.h"

#if defined(CESIUM_GD_MODULE)
#include "scene/resources/material.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/image_texture.hpp"
#include "godot_cpp/classes/material.hpp"
#include "godot_cpp/classes/shader.hpp"
#include "godot_cpp/classes/shader_material.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include "godot_cpp/classes/ref.hpp"
using namespace godot;
#endif

#include <cstdint>
#include <string>
#include <unordered_map>

struct CesiumGltfPrimitiveMaterialOptions {
	bool pointPrimitive = false;
	bool forceUnshaded = false;
};

/**
 * Converts glTF 2.0 metallic/roughness materials into Godot shader materials.
 *
 * Godot counterpart of Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/CesiumGltfComponent.cpp
 * - Source/CesiumRuntime/Private/CesiumTextureUtility.cpp
 * - Source/CesiumRuntime/Private/GenerateMaterialUtility.cpp
 *
 * One loader is used for one model. Decoded images and shader variants are
 * shared, while each glTF material keeps its own uniform values.
 */
class CesiumGltfMaterialLoader {
public:
	CesiumGltfMaterialLoader(
		const CesiumGltf::Model& model,
		const std::shared_ptr<CesiumGltfImageAssetResourceCache>& sharedImageCache,
		const std::unordered_map<
			const CesiumImage::ImageAsset*,
			CesiumGltfImageContentFingerprint
		>* imageContentFingerprints,
		std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>*
			sharedImageResources,
		bool enableLodTransitionDither,
		bool enableTranslucencyDepthPrepass
	);

	Ref<Material> create_material(
		const CesiumGltf::Material* material,
		const CesiumFeatureStyleEncoding* featureStyle,
		const CesiumGltfPrimitiveMaterialOptions& primitiveOptions,
		Error* error
	);

private:
	enum class WrapMode : int32_t {
		Clamp = 0,
		Repeat = 1,
		MirroredRepeat = 2
	};

	struct SamplerState {
		std::string shaderFilterHint = "filter_linear_mipmap";
		WrapMode wrapS = WrapMode::Repeat;
		WrapMode wrapT = WrapMode::Repeat;
		bool exactFilter = true;
	};

	struct TextureSlot {
		bool present = false;
		Ref<ImageTexture> texture;
		SamplerState sampler;
		int32_t textureCoordinateSet = 0;
		Vector2 offset = Vector2(0.0, 0.0);
		Vector2 scale = Vector2(1.0, 1.0);
		double rotation = 0.0;
	};

	TextureSlot load_texture_slot(
		const CesiumGltf::TextureInfo* textureInfo,
		Error* error
	);
	Ref<ImageTexture> load_texture(
		int32_t textureIndex,
		Error* error
	);
	SamplerState get_sampler_state(int32_t textureIndex) const;
	Ref<Shader> get_or_create_shader(
		const CesiumGltf::Material& material,
		const TextureSlot& baseColor,
		const TextureSlot& metallicRoughness,
		const TextureSlot& normal,
		const TextureSlot& occlusion,
		const TextureSlot& emissive,
		const CesiumFeatureStyleEncoding* featureStyle,
		const CesiumGltfPrimitiveMaterialOptions& primitiveOptions
	);
	std::string build_shader_code(
		const CesiumGltf::Material& material,
		const TextureSlot& baseColor,
		const TextureSlot& metallicRoughness,
		const TextureSlot& normal,
		const TextureSlot& occlusion,
		const TextureSlot& emissive,
		const CesiumFeatureStyleEncoding* featureStyle,
		const CesiumGltfPrimitiveMaterialOptions& primitiveOptions
	);
	static void configure_texture_slot(
		const Ref<ShaderMaterial>& material,
		const char* prefix,
		const TextureSlot& slot
	);

	const CesiumGltf::Model& m_model;
	std::shared_ptr<CesiumGltfImageAssetResourceCache> m_sharedImageCache;
	const std::unordered_map<
		const CesiumImage::ImageAsset*,
		CesiumGltfImageContentFingerprint
	>* m_imageContentFingerprints = nullptr;
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>*
		m_sharedImageResources = nullptr;
	bool m_enableLodTransitionDither = false;
	bool m_enableTranslucencyDepthPrepass = true;
	std::unordered_map<const CesiumImage::ImageAsset*, Ref<ImageTexture>>
		m_textureCache;
	std::unordered_map<std::string, Ref<Shader>> m_shaderCache;
};

#endif // CESIUM_GLTF_MATERIAL_LOADER_H
