/*
 * Godot adaptation of Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/CesiumGltfComponent.cpp
 * - Source/CesiumRuntime/Private/CesiumGltfPrimitiveComponent.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0.
 */
#ifndef CESIUM_GD_MODEL_LOADER_H
#define CESIUM_GD_MODEL_LOADER_H

#include "CesiumGltf/Model.h"
#include "CesiumGltfReader/GltfReader.h"
#include "Runtime/Private/Metadata/CesiumMetadataSnapshot.h"
#include "Runtime/Private/Metadata/CesiumPrimitiveFeaturesSnapshot.h"
#include "Runtime/Private/Metadata/CesiumPrimitiveMetadataSnapshot.h"
#include "Runtime/Private/Metadata/CesiumFeatureStyleEncoding.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Runtime/Private/Renderer/CesiumGltfModelResourceCache.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveMetadata.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "glm/ext/matrix_double4x4.hpp"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/classes/multi_mesh.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/templates/vector.hpp"
#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/variant/aabb.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/vector4.hpp"
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "scene/resources/mesh.h"
#endif

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CesiumGltfMaterialLoader;

struct CesiumGDPrimitiveInstance {
	int32_t nodeIndex = -1;
	int32_t meshIndex = -1;
	int32_t primitiveIndex = -1;
	glm::dmat4 transform = glm::dmat4(1.0);
};

/**
 * CPU-only geometry produced from one glTF primitive instance.
 *
 * This type deliberately contains no Godot Object, Resource, RID, or scene
 * node. It may therefore be built and discarded on Cesium's load workers
 * without touching RenderingServer ownership.
 */
struct CesiumGDPreparedPrimitive {
	CesiumGDPrimitiveInstance source;
	bool isGpuInstanced = false;
	bool isTranslucent = false;
	bool requiresUnculledInstancing = false;
	int32_t instanceCount = 0;
	std::vector<float> instanceTransformBuffer;
	AABB instanceBounds;
	Vector<Vector3> vertices;
	Vector<int32_t> indices;
	Vector<Vector3> normals;
	Vector<Vector4> tangents;
	Vector<Color> colors;
	Vector<Vector2> textureCoords;
	Vector<Vector2> textureCoords2;
	Vector<Vector3> encodedFeatureIds;
	Vector3 dimensions;
	int64_t pointCount = 0;
	int64_t pointDiameter = 0;
	std::vector<float> encodedInstanceFeatureIds;
	CesiumFeatureStyleEncoding featureStyleEncoding;
	std::shared_ptr<CesiumPrimitiveFeaturesSnapshot> featuresSnapshot;
	std::shared_ptr<CesiumPrimitiveFeaturesSnapshot> instanceFeaturesSnapshot;
	std::shared_ptr<CesiumPrimitiveMetadataSnapshot> metadataSnapshot;
	uint64_t geometryBytes = 0;
	uint64_t textureBytes = 0;
	Ref<Material> realizedMaterial;
	Ref<ArrayMesh> realizedMesh;
	Ref<MultiMesh> realizedMultiMesh;
	Ref<CesiumPrimitiveFeatures> realizedInstanceFeatures;
	Ref<CesiumPrimitiveMetadata> realizedPrimitiveMetadata;
	int32_t realizedSurfaceIndex = -1;
};

enum class CesiumGDRealizationFailureStage {
	Renderer,
	Material,
};

struct CesiumGDPreparedModel {
	std::vector<CesiumGDPreparedPrimitive> primitives;
	uint64_t geometryBytes = 0;
	uint64_t textureBytes = 0;
	uint64_t maximumPrimitiveGeometryBytes = 0;
	uint64_t maximumPrimitiveTextureBytes = 0;
	size_t nextPrimitiveToRealize = 0;
	Ref<ArrayMesh> realizedMesh;
	std::shared_ptr<CesiumGltfMaterialLoader> materialLoader;
	std::unordered_map<int64_t, Ref<Material>> realizedMaterials;
	uint64_t realizationMicroseconds = 0;
	CesiumGDRealizationFailureStage realizationFailureStage =
		CesiumGDRealizationFailureStage::Renderer;
	std::optional<int32_t> pendingMaterialWarningError;
	std::shared_ptr<CesiumModelMetadataSnapshot> metadataSnapshot;
	Ref<CesiumModelMetadata> realizedMetadata;
	std::shared_ptr<CesiumGltfImageAssetResourceCache> sharedImageCache;
	std::unordered_map<
		const CesiumImage::ImageAsset*,
		CesiumGltfImageContentFingerprint
	> imageContentFingerprints;
	std::unordered_set<const CesiumImage::ImageAsset*> cpuImageAssets;
	std::shared_ptr<CesiumGltfModelResourceCache> sharedModelCache;
	std::shared_ptr<CesiumGltfSharedModelResource> sharedModelResource;
	std::string contentKey;
	bool enableLodTransitionDither = false;
	bool enableTranslucencyDepthPrepass = true;
	bool sharedModelLookupComplete = false;
	std::vector<std::shared_ptr<CesiumGltfSharedImageResource>>
		sharedImageResources;
	size_t nextMetadataEnumToRealize = 0;
	Ref<CesiumPropertyTable> currentMetadataTable;
	Ref<CesiumPropertyTexture> currentMetadataTexture;
	size_t nextMetadataTableToRealize = 0;
	size_t nextMetadataPropertyToRealize = 0;
	size_t nextMetadataTextureToRealize = 0;
	size_t nextMetadataTexturePropertyToRealize = 0;
	bool metadataRealizationComplete = false;
	std::vector<Ref<CesiumPrimitiveFeatures>> realizedPrimitiveFeatures;
	Ref<CesiumPrimitiveFeatures> currentPrimitiveFeatures;
	size_t nextPrimitiveFeaturesToRealize = 0;
	size_t nextFeatureIdSetToRealize = 0;
	Ref<CesiumPrimitiveFeatures> currentInstanceFeatures;
	std::unordered_map<
		const CesiumPrimitiveFeaturesSnapshot*,
		Ref<CesiumPrimitiveFeatures>
	> realizedInstanceFeaturesBySnapshot;
	size_t nextInstanceFeaturesToRealize = 0;
	size_t nextInstanceFeatureIdSetToRealize = 0;
	size_t nextPrimitiveMetadataToRealize = 0;
	size_t nextPropertyAttributeToRealize = 0;
	size_t nextPropertyAttributePropertyToRealize = 0;
	Ref<CesiumPrimitiveMetadata> currentPrimitiveMetadata;
	Ref<CesiumPropertyAttribute> currentPropertyAttribute;
};

/**
 * Converts a post-processed Cesium Native glTF model into Godot mesh surfaces.
 *
 * This is the Godot renderer boundary corresponding to Cesium for Unreal
 * v2.29.0's CesiumGltfComponent / CesiumGltfPrimitiveComponent. Accessors are
 * read through Cesium Native AccessorView so sparse data, byte strides, and
 * normalized integer texture coordinates are handled by one validated path.
 */
class CesiumGDModelLoader {
public:
	/** Performs accessor decoding and geometry transforms without GPU objects. */
	static CesiumGDPreparedModel* prepare_model(
		const CesiumGltf::Model& model,
		Error* error,
		const CesiumFeatureStyleEncodingDescription& featureStyle =
			CesiumFeatureStyleEncodingDescription()
	);

	/** Creates Godot meshes, textures, shaders, and materials on the main thread. */
	static Ref<ArrayMesh> realize_prepared_model(
		const CesiumGltf::Model& model,
		CesiumGDPreparedModel& prepared,
		Error* error
	);

	/** Realizes one material or primitive surface step and reports completion. */
	static bool realize_prepared_model_incrementally(
		const CesiumGltf::Model& model,
		CesiumGDPreparedModel& prepared,
		Error* error
	);

	static Ref<ArrayMesh> generate_meshes_from_model(
		const CesiumGltf::Model& model,
		Error* error
	);
	static std::vector<CesiumGDPrimitiveInstance> collect_primitive_instances(
		const CesiumGltf::Model& model
	);

	/** Maps realized glTF texture-coordinate semantics to Godot UV channels. */
	static Dictionary get_texture_coordinate_mappings(
		const CesiumGltf::MeshPrimitive& primitive
	);

	static glm::dmat4 apply_rtc_center(
		const CesiumGltf::Model& gltf,
		const glm::dmat4x4& rootTransform
	);
	static glm::dmat4 apply_gltf_up_axis_transform(
		const CesiumGltf::Model& model,
		const glm::dmat4x4& rootTransform
	);
	static Error parse_gltf(
		const String& assetPath,
		CesiumGltfReader::GltfReaderResult* out
	);

private:
	static constexpr Mesh::PrimitiveType cesium_to_godot_primitive_mode(
		int32_t mode
	);
	static Error apply_surface_to_mesh(
		const CesiumGltf::MeshPrimitive& primitive,
		Ref<ArrayMesh>& mesh,
		const Array& arrays,
		bool hasEncodedFeatureIds
	);

#if defined(CESIUM_GD_EXT)
	static Array generate_array_mesh_ext(
		const Vector<Vector3>& vertices,
		const Vector<int32_t>& indices,
		const Vector<Vector3>& normals,
		const Vector<Vector4>& tangents,
		const Vector<Color>& colors,
		const Vector<Vector2>& textureCoords,
		const Vector<Vector2>& textureCoords2,
		const Vector<Vector3>& encodedFeatureIds
	);
#endif

	static Vector<Vector3> get_positions(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		Error* error
	);
	static Vector<Vector3> get_normals(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		Error* error
	);
	static Vector<Vector4> get_tangents(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		Error* error
	);
	static Vector<Color> get_colors(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		Error* error
	);
	static Vector<Vector2> get_texture_coordinates(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		const std::string& semantic,
		Error* error
	);
	static Vector<int32_t> get_index_buffer_from_primitive(
		const CesiumGltf::MeshPrimitive& primitive,
		const CesiumGltf::Model& model,
		int32_t vertexCount,
		Error* error
	);
	static Error validate_attribute_count(
		int32_t vertexCount,
		int32_t attributeCount,
		const char* semantic
	);
	static Error generate_normals(
		Vector<Vector3>* normalBuffer,
		const Vector<Vector3>& vertexBuffer,
		const Vector<int32_t>& indexBuffer
	);
};

#endif // CESIUM_GD_MODEL_LOADER_H
