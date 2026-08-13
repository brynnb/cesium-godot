/*
 * Godot adaptation of Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/Cesium3DTileset.cpp
 * Source/CesiumRuntime/Private/CesiumViewExtension.cpp
 */
#include "Cesium3DTilesSelection/BoundingVolume.h"
#include "Cesium3DTilesSelection/ViewState.h"
#include "CesiumGeometry/BoundingCylinderRegion.h"
#include "CesiumGeometry/BoundingSphere.h"
#include "CesiumGeometry/OrientedBoundingBox.h"
#include "CesiumGeospatial/BoundingRegion.h"
#include "CesiumGeospatial/BoundingRegionWithLooseFittingHeights.h"
#include "CesiumGeospatial/S2CellBoundingVolume.h"
#include "CesiumUtility/IntrusivePointer.h"
#include "Models/CesiumDataSource.h"
#include "Godot/Nodes/CesiumGeoreferencedMesh.h"
#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/vector_double3.hpp"
#include "godot_cpp/classes/geometry_instance3d.hpp"
#include "godot_cpp/variant/basis.hpp"
#include "godot_cpp/variant/callable.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/quaternion.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include <cstdint>
#include <type_traits>
#define SPDLOG_COMPILED_LIB
#include "Godot/Nodes/CesiumGeoreference.h"
#define SPDLOG_FMT_EXTERNAL

#include "Godot/Nodes/CesiumGDCreditSystem.h"
#include "Runtime/Public/Cesium3DTilesetLifecycleEventReceiver.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/Renderer/CesiumPointCloudShading.h"
#include "Runtime/Public/Metadata/CesiumMetadataStyle.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"
#include "Runtime/Public/Georeference/CesiumSampleHeightResult.h"
#include "Runtime/Public/Georeference/CesiumSampleHeightMostDetailedRequest.h"
#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"
#include "Runtime/Public/Diagnostics/CesiumTileDebugState.h"
#include "Runtime/Private/Bounds/CesiumBoundingVolumeSnapshot.h"
#include "Runtime/Private/Georeference/CesiumGodotCameraProjection.h"
#include "Runtime/Public/CesiumCameraManager.h"
#include "Runtime/Private/Materials/CesiumLodTransitionController.h"
#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"
#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"
#include "Runtime/Private/Renderer/CesiumGDModelLoader.h"
#include "missing_functions.hpp"
#include "Cesium3DTilesSelection/Tile.h"
#include "Cesium3DTilesSelection/TileContent.h"
#include "Cesium3DTilesSelection/TileLoadFailureDetails.h"
#include "Cesium3DTilesSelection/TileID.h"
#include "Cesium3DTilesSelection/SampleHeightResult.h"
#include "CesiumGDTileset.h"

#if defined(CESIUM_GD_EXT)
#include "godot_cpp/core/property_info.hpp"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/collision_object3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include "scene/resources/mesh.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "core/error/error_macros.h"
#endif


#include "Models/Cesium3DTile.h"
#include "Utils/AssetManipulation.h"
#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"
#include "Cesium3DTilesSelection/Tileset.h"
#include "Cesium3DTilesSelection/TilesetExternals.h"
#include "Runtime/Private/Async/GodotTaskProcessor.h"
#include "Runtime/Private/Networking/InstrumentedCacheDatabase.h"
#include "Utils/CesiumMathUtils.h"
#include "Runtime/Private/Networking/NetworkAssetAccessor.h"
#include "Runtime/Private/Renderer/GodotPrepareRenderResources.h"
#include "Cesium3DTilesContent/registerAllTileContentTypes.h"
#include "Utils/CesiumVariantHash.h"
#include <glm/gtc/quaternion.hpp>
#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"
#include "CesiumAsync/GunzipAssetAccessor.h"
#include "CesiumAsync/IAssetResponse.h"
#include <CesiumAsync/CachingAssetAccessor.h>
#include "CesiumAsync/SqliteCache.h"

#include "CesiumGltf/Material.h"
#include "CesiumGltf/ExtensionKhrTextureTransform.h"
#include "CesiumGltf/Mesh.h"
#include "CesiumGltf/MeshPrimitive.h"
#include "CesiumUtility/JsonValue.h"
#include "CesiumUtility/Math.h"

#include <any>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <limits>
#include <numbers>
#include <thread>
#include <unordered_map>

constexpr real_t DEFAULT_MAIN_THREAD_BUDGET_MILLISECONDS = 5.0;
constexpr real_t DEFAULT_CACHE_UNLOAD_BUDGET_MILLISECONDS = 5.0;
constexpr const char* ION_ACCESS_TOKEN_P_NAME = "ion_access_token";
constexpr const char* ION_ASSET_ID_P_NAME = "ion_asset_id";
constexpr const char* URL_P_NAME = "url";

constexpr const char* REQUEST_HEADERS_DESC =
	"HTTP headers attached to the root tileset request and every dependent "
	"content request. Header names are normalized to lowercase and surrounding "
	"whitespace is removed. Empty, non-string, and line-breaking entries are "
	"ignored. Values are intentionally excluded from diagnostics.";

constexpr const char* MAXIMUM_SCREEN_SPACE_DESC = "The maximum number of pixels of error when rendering this tileset.\nThis is used to select an appropriate level-of-detail.\n\nWhen a tileset uses the older layer.json / quantized-mesh format rather than 3D Tiles, this value is effectively divided by 8.0.\nSo the default value of 16.0 corresponds to the standard value for quantized-mesh terrain of 2.0";
constexpr const char* MAXIMUM_SIMULTANEOUS_TILE_LOADS_DESC = "The maximum number of tiles that may simultaneously be in the process of loading.";
constexpr const char* STALE_REQUEST_CANCELLATION_DESC = "Abort in-flight HTTP and skip CPU/GPU preparation when no current or predictive view needs a tile anymore.";
constexpr const char* PRELOAD_ANCESTORS_DESC = "Indicates whether the ancestors of rendered tiles should be preloaded.\nSetting this to true optimizes the zoom-out experience and provides more detail in newly-exposed areas when panning.\nThe down side is that it requires loading more tiles";
constexpr const char* PRELOAD_SIBLINGS_DESC = "Indicates whether the siblings of rendered tiles should bepreloaded.\nSetting this to true causes tiles with the same parent as arendered tile to be loaded, even if they are culled.\nSetting this to truemay provide a better panning experience at the cost of loading more tiles.";
constexpr const char* LOADING_DESCENDANT_LIMIT_DESC = "The number of loading descendant tiles that is considered \"too many\".\nIf a tile has too many loading descendants, that tile will be loaded and rendered before any of its descendants are loaded and rendered. \nThis means more feedback for the user that something is happening at the cost of a longer overall load time.\nSetting this to 0 will cause each tile level to be loaded successively, significantly increasing load time.\nSetting it to a large number (e.g. 1000) will minimize the number of tiles that are loaded but tend to make detail appear all at once after a long wait.";
constexpr const char* FORBID_HOLES_DESC = "Never render a tileset with missing tiles.\n\nWhen true, the tileset will guarantee that the tileset will never be rendered with holes in place of tiles that are not yet loaded.\nIt does this by refusing to refine a parent tile until all of its child tiles are ready to render.\nThus, when the camera moves, we will always have something - even if it's low resolution - to render any part of the tileset that becomes visible.\nWhen false, overall loading will be faster, but newly-visible parts of the tileset may initially be blank.";
constexpr const char* FRUSTUM_CULLING_DESC = "Cull 3D Tiles outside the active Godot camera frustum. The adapter preserves perspective KEEP_WIDTH / KEEP_HEIGHT, orthographic, and asymmetric-frustum projection semantics.";
constexpr const char* FOG_CULLING_DESC = "Use Cesium Native's ellipsoid-height fog table to cull fully obscured distant tiles. This is independent of Godot Environment fog and is usually inappropriate for a local-Cartesian world.";
constexpr const char* ENFORCE_CULLED_SSE_DESC = "When a culling stage is disabled, continue refining tiles that it would otherwise cull until culled_screen_space_error is reached.";
constexpr const char* RENDER_TILES_UNDER_CAMERA_DESC = "Keep region-bounded terrain directly beneath the camera available even outside its view frustum. Cesium Native applies this only to geographic region bounds.";
constexpr const char* LOD_TRANSITIONS_DESC = "Retain old and new LOD tiles for Cesium Native's timed transition and apply an opaque placement-local dither to compatible materials. Changing this rebuilds the tileset so generated shaders remain zero-cost when disabled.";
constexpr const char* TRANSLUCENCY_DEPTH_PREPASS_DESC = "Use Godot's alpha depth prepass for glTF BLEND materials. Fully opaque texels write depth before the transparent pass, improving mostly-opaque foliage and facade materials while preserving partial transparency. Changing this rebuilds generated shaders.";
constexpr const char* KICK_DESCENDANTS_WHILE_FADING_DESC = "Keep descendants off the render list while an ancestor is still fading in, preventing the ancestor from popping directly to full visibility.";
constexpr const char* GENERATE_MISSING_NORMALS_DESC = "Whether to generate smooth normals when normals are missing in theoriginal Gltf.\n\nAccording to the Gltf spec: \"When normals are not specified, clientimplementations should calculate flat normals.\"\nHowever, calculating flatnormals requires duplicating vertices.\nThis option allows the gltfs to besent with explicit smooth normals when the original gltf was missingnormals.";
constexpr const char* MOVEMENT_PREDICTION_ENABLED_DESC = "Select tiles from a second, non-rendered future camera view while the camera is moving. Current visible work keeps the higher scheduler weight; prediction uses otherwise available loading capacity.";
constexpr const char* TURN_PREDICTION_ENABLED_DESC = "Extend the non-rendered future view toward the camera's filtered turn direction so tiles can warm before they enter the visible frustum.";
constexpr const char* ZOOM_OUT_PREDICTION_ENABLED_DESC = "Expand the non-rendered future view while the camera is zooming out so newly exposed tiles can warm before they become visible.";
constexpr const char* AUTOMATIC_HARDWARE_BUDGETS_DESC = "Opt in to a bounded cache recommendation based on system RAM and, when the running Godot exposes it, total device memory. Explicitly setting maximum_cached_bytes disables automatic sizing.";
constexpr const char* OCCLUSION_CULLING_UNAVAILABLE_REASON =
	"Godot 4.6 public RenderingServer and RenderingDevice APIs do not "
	"expose the per-bounds renderer occlusion result required by Cesium Native.";

namespace {
PackedFloat64Array vector_components(const glm::dvec3& value) {
	PackedFloat64Array result;
	result.push_back(value.x);
	result.push_back(value.y);
	result.push_back(value.z);
	return result;
}

std::string get_tile_hierarchy_path(
	const Cesium3DTilesSelection::Tile* tile
) {
	if (tile == nullptr) {
		return std::string();
	}
	std::vector<size_t> reversedIndices;
	const Cesium3DTilesSelection::Tile* current = tile;
	while (current->getParent() != nullptr) {
		const Cesium3DTilesSelection::Tile* parent = current->getParent();
		const auto children = parent->getChildren();
		size_t childIndex = std::numeric_limits<size_t>::max();
		for (size_t index = 0; index < children.size(); ++index) {
			if (&children[index] == current) {
				childIndex = index;
				break;
			}
		}
		reversedIndices.push_back(childIndex);
		current = parent;
	}
	std::string result("root");
	for (auto it = reversedIndices.rbegin(); it != reversedIndices.rend(); ++it) {
		result.push_back('/');
		result += *it == std::numeric_limits<size_t>::max()
			? std::string("?")
			: std::to_string(*it);
	}
	return result;
}

String resolve_cache_path(const String& configuredPath) {
	if (configuredPath.is_empty()) {
		return String();
	}
	return ProjectSettings::get_singleton()->globalize_path(configuredPath);
}

uint64_t file_size_if_present(const String& path) {
	if (path.is_empty()) {
		return 0;
	}
	std::error_code error;
	const uintmax_t size = std::filesystem::file_size(
		std::filesystem::path(path.utf8().get_data()),
		error
	);
	return error ? 0 : static_cast<uint64_t>(size);
}

uint64_t sqlite_files_size(const String& databasePath) {
	return file_size_if_present(databasePath) +
		file_size_if_present(databasePath + String("-wal")) +
		file_size_if_present(databasePath + String("-shm"));
}

using CesiumGodotMetadataConversions::json_object_to_dictionary;
using CesiumGodotMetadataConversions::json_value_to_variant;

Array doubles_to_array(const std::vector<double>& values) {
	Array result;
	for (double value : values) {
		result.push_back(value);
	}
	return result;
}

Dictionary texture_info_to_dictionary(const CesiumGltf::TextureInfo& texture) {
	Dictionary result;
	result["index"] = texture.index;
	result["texcoord"] = texture.texCoord;
	result["effective_texcoord"] = texture.texCoord;
	const CesiumGltf::ExtensionKhrTextureTransform* transform =
		texture.getExtension<CesiumGltf::ExtensionKhrTextureTransform>();
	if (transform != nullptr) {
		Dictionary transformInfo;
		transformInfo["offset"] = doubles_to_array(transform->offset);
		transformInfo["rotation"] = transform->rotation;
		transformInfo["scale"] = doubles_to_array(transform->scale);
		if (transform->texCoord.has_value()) {
			transformInfo["texcoord"] = static_cast<int64_t>(
				*transform->texCoord
			);
			result["effective_texcoord"] = static_cast<int64_t>(
				*transform->texCoord
			);
		}
		result["texture_transform"] = transformInfo;
	}
	result["extras"] = json_object_to_dictionary(texture.extras);
	return result;
}

Dictionary gltf_material_to_dictionary(const CesiumGltf::Material* material) {
	Dictionary result;
	if (material == nullptr) {
		result["defined"] = false;
		return result;
	}

	result["defined"] = true;
	result["name"] = String(material->name.c_str());
	result["alpha_mode"] = String(material->alphaMode.c_str());
	result["alpha_cutoff"] = material->alphaCutoff;
	result["double_sided"] = material->doubleSided;
	result["emissive_factor"] = doubles_to_array(material->emissiveFactor);
	result["extras"] = json_object_to_dictionary(material->extras);
	result["unknown_properties"] = json_object_to_dictionary(material->unknownProperties);

	Array extensionNames;
	Dictionary genericExtensions;
	for (const auto& [name, extension] : material->extensions) {
		extensionNames.push_back(String(name.c_str()));
		const CesiumUtility::JsonValue* generic = std::any_cast<CesiumUtility::JsonValue>(&extension);
		if (generic != nullptr) {
			genericExtensions[String(name.c_str())] = json_value_to_variant(*generic);
		}
	}
	result["extension_names"] = extensionNames;
	result["generic_extensions"] = genericExtensions;

	if (material->pbrMetallicRoughness) {
		const CesiumGltf::MaterialPBRMetallicRoughness& pbr = *material->pbrMetallicRoughness;
		Dictionary pbrInfo;
		pbrInfo["base_color_factor"] = doubles_to_array(pbr.baseColorFactor);
		pbrInfo["metallic_factor"] = pbr.metallicFactor;
		pbrInfo["roughness_factor"] = pbr.roughnessFactor;
		if (pbr.baseColorTexture) {
			pbrInfo["base_color_texture"] = texture_info_to_dictionary(*pbr.baseColorTexture);
		}
		if (pbr.metallicRoughnessTexture) {
			pbrInfo["metallic_roughness_texture"] = texture_info_to_dictionary(*pbr.metallicRoughnessTexture);
		}
		result["pbr_metallic_roughness"] = pbrInfo;
	}
	if (material->normalTexture) {
		Dictionary normalInfo = texture_info_to_dictionary(*material->normalTexture);
		normalInfo["scale"] = material->normalTexture->scale;
		result["normal_texture"] = normalInfo;
	}
	if (material->occlusionTexture) {
		Dictionary occlusionInfo = texture_info_to_dictionary(*material->occlusionTexture);
		occlusionInfo["strength"] = material->occlusionTexture->strength;
		result["occlusion_texture"] = occlusionInfo;
	}
	if (material->emissiveTexture) {
		result["emissive_texture"] = texture_info_to_dictionary(*material->emissiveTexture);
	}
	return result;
}

bool is_header_string(const Variant& value) {
	return value.get_type() == Variant::STRING ||
		value.get_type() == Variant::STRING_NAME;
}

bool contains_header_line_break(const String& value) {
	return value.contains("\r") || value.contains("\n");
}

bool is_valid_http_header_name(const String& value) {
	if (value.is_empty()) {
		return false;
	}
	for (int64_t index = 0; index < value.length(); ++index) {
		const char32_t character = value[index];
		const bool isAsciiLetter =
			(character >= U'a' && character <= U'z') ||
			(character >= U'A' && character <= U'Z');
		const bool isDigit = character >= U'0' && character <= U'9';
		const bool isTokenPunctuation =
			character == U'!' || character == U'#' || character == U'$' ||
			character == U'%' || character == U'&' || character == U'\'' ||
			character == U'*' || character == U'+' || character == U'-' ||
			character == U'.' || character == U'^' || character == U'_' ||
			character == U'`' || character == U'|' || character == U'~';
		if (!isAsciiLetter && !isDigit && !isTokenPunctuation) {
			return false;
		}
	}
	return true;
}

Dictionary normalize_request_headers(const Dictionary& headers) {
	Dictionary result;
	const Array keys = headers.keys();
	for (int32_t index = 0; index < keys.size(); ++index) {
		const Variant keyVariant = keys[index];
		const Variant valueVariant = headers[keyVariant];
		if (!is_header_string(keyVariant) || !is_header_string(valueVariant)) {
			continue;
		}

		const String key = String(keyVariant).strip_edges().to_lower();
		const String value = String(valueVariant).strip_edges();
		if (
			!is_valid_http_header_name(key) || value.is_empty() ||
			contains_header_line_break(value)
		) {
			continue;
		}
		result[key] = value;
	}
	return result;
}

bool request_headers_equal(
	const Dictionary& left,
	const Dictionary& right
) {
	if (left.size() != right.size()) {
		return false;
	}
	const Array keys = left.keys();
	for (int32_t index = 0; index < keys.size(); ++index) {
		const Variant key = keys[index];
		if (!right.has(key) || String(left[key]) != String(right[key])) {
			return false;
		}
	}
	return true;
}

std::vector<CesiumAsync::IAssetAccessor::THeader>
request_headers_to_native(const Dictionary& headers) {
	std::vector<CesiumAsync::IAssetAccessor::THeader> result;
	const Array keys = headers.keys();
	result.reserve(static_cast<size_t>(keys.size()));
	for (int32_t index = 0; index < keys.size(); ++index) {
		const String key = String(keys[index]);
		const String value = String(headers[keys[index]]);
		const CharString keyUtf8 = key.utf8();
		const CharString valueUtf8 = value.utf8();
		result.emplace_back(
			std::string(keyUtf8.get_data(), keyUtf8.length()),
			std::string(valueUtf8.get_data(), valueUtf8.length())
		);
	}
	return result;
}
}


/**
* @brief This will be the underlying config for the tileset
* the class basically acts as a builder wrapper to provide
* UI serialization in-engine
*/
class OpaqueTilesetOptions {
public:
	OpaqueTilesetOptions() = default;
	~OpaqueTilesetOptions() = default;
	Cesium3DTilesSelection::TilesetOptions options{};
};

inline void extract_properties_from_bounding_box(const CesiumGeometry::OrientedBoundingBox& box, Dictionary* refProperties, const CesiumGeoreference* georeference) {
	const Transform3D& ecefToEngineXform = georeference->get_tx_ecef_to_engine();

	Vector3 size = CesiumMathUtils::from_glm_vec3(box.getLengths());
	Vector3 center = CesiumMathUtils::from_glm_vec3(box.getCenter());
	const glm::mat3& halfAxes = box.getHalfAxes();

	const glm::dvec3& basisX = halfAxes[0];
	const glm::dvec3& basisY = halfAxes[1];
	const glm::dvec3& basisZ = halfAxes[2];
	Basis basisLocal = Basis(
		CesiumMathUtils::from_glm_vec3(basisX),
		CesiumMathUtils::from_glm_vec3(basisY),
		CesiumMathUtils::from_glm_vec3(basisZ)
	).orthonormalized();

	Transform3D xform = ecefToEngineXform * Transform3D(basisLocal, center);

	if( !refProperties->has("size") ) {
		(*refProperties)["size"] = size;
	}
	if( !refProperties->has("transform") ) {
		(*refProperties)["transform"] = xform;
	}
}

inline void draw_debug_volume_from_variant(const Cesium3DTilesSelection::BoundingVolume& boundingVariant, const Callable& callback, const CesiumGeoreference* georeference) {
	// Determine the type of bounding volume
	size_t typeIndex = boundingVariant.index();
	EBoundingType boundingType = static_cast<EBoundingType>(typeIndex + 1);
	Dictionary properties{};

	// Extract our rotation here
	const Transform3D& ecefToEngineXform = georeference->get_tx_ecef_to_engine();

	switch (boundingType) {
        case EBoundingType::Sphere:
        	{
	    		const auto& sphere = std::get<CesiumGeometry::BoundingSphere>(boundingVariant);
				Vector3 center = ecefToEngineXform.xform(CesiumMathUtils::from_glm_vec3(sphere.getCenter()));
				if( !properties.has("center") ){
					properties["center"] = center;
				}
				if( !properties.has("radius") ){
					properties["radius"] = sphere.getRadius();
				}
			}
			break;
        case EBoundingType::Box:
        	{
	        	const auto& box = std::get<CesiumGeometry::OrientedBoundingBox>(boundingVariant);
				extract_properties_from_bounding_box(box, &properties, georeference);
        	}
			break;
        case EBoundingType::RegionWithLooseFittingHeights:
        	{
        		const auto& boundingRegionLoose = std::get<CesiumGeospatial::BoundingRegionWithLooseFittingHeights>(boundingVariant);
        		const CesiumGeometry::OrientedBoundingBox& box = boundingRegionLoose.getBoundingRegion().getBoundingBox();
        		extract_properties_from_bounding_box(box, &properties, georeference);
        	}
			break;
        case EBoundingType::Region:
	        	{
		        	const auto& region = std::get<CesiumGeospatial::BoundingRegion>(boundingVariant);
		        	const CesiumGeometry::OrientedBoundingBox& box = region.getBoundingBox();
		        	extract_properties_from_bounding_box(box, &properties, georeference);
	        	}
        	break;
        case EBoundingType::CellVolume:
        case EBoundingType::CylinderRegion:
      	default:
      		{
				ERR_PRINT("NOT YET IMPLEMENTED");
      		}
        	break;
      		// Not identified, send in debug
    }
	
	// Call the GDScript function
	godot::Array callback_args;
	callback_args.push_back(static_cast<int32_t>(boundingType));
	callback_args.push_back(properties);
	callback.callv(callback_args);
}

Cesium3DTileset::Cesium3DTileset()
{
	this->m_initialLoadingFinished = false;
	this->m_tilesetConfig = new OpaqueTilesetOptions();
	//Set all the default values for the tileset options that are not exposed to the editor
	this->m_tilesetConfig->options.mainThreadLoadingTimeLimit =
		DEFAULT_MAIN_THREAD_BUDGET_MILLISECONDS;
	this->m_tilesetConfig->options.tileCacheUnloadTimeLimit =
		DEFAULT_CACHE_UNLOAD_BUDGET_MILLISECONDS;
	this->m_tilesetConfig->options.lodTransitionLength = 0.5f;
	// Godot exposes no renderer-independent per-object hardware occlusion query
	// API suitable for Native's proxy contract. Keep this disabled until the
	// dedicated Godot renderer bridge is installed; reporting it as enabled with
	// a null proxy pool would be misleading even though Native safely no-ops.
	this->m_tilesetConfig->options.enableOcclusionCulling = false;
	this->m_runtimeStatistics =
		std::make_shared<CesiumTilesetRuntimeStatistics>();
	this->m_loadFailureQueue = std::make_shared<CesiumLoadFailureQueue>();
	this->m_pointCloudShading.instantiate();
	this->m_pointCloudShading->connect(
		"changed",
		Callable(this, "_on_point_cloud_shading_changed")
	);
	this->m_tilesetConfig->options.contentOptions.applyTextureTransform = false;
	const uint32_t hardwareThreads = std::thread::hardware_concurrency();
	this->m_workerThreadCount = std::clamp<uint32_t>(
		hardwareThreads > 1 ? hardwareThreads - 1 : 1,
		1,
		8
	);
	const CesiumImage::SupportedGpuCompressedPixelFormats supportedFormats =
		CesiumGDTextureLoader::get_supported_gpu_compressed_pixel_formats();
	this->m_tilesetConfig->options.contentOptions.ktx2TranscodeTargets = {
		supportedFormats,
		true
	};

	const std::shared_ptr<CesiumLoadFailureQueue> failureQueue =
		this->m_loadFailureQueue;
	this->m_tilesetConfig->options.loadErrorCallback = [failureQueue](
		const Cesium3DTilesSelection::TilesetLoadFailureDetails& failData
	) {
		ERR_PRINT(
			String("Failed to load a given tileset, error: ") +
			failData.message.c_str()
		);
		if (failureQueue == nullptr) {
			return;
		}
		CesiumLoadFailureRecord record;
		record.category = CesiumLoadFailure::Category::Tileset;
		record.stage =
			failData.type == Cesium3DTilesSelection::TilesetLoadType::CesiumIon
			? CesiumLoadFailure::Stage::IonEndpoint
			: failData.type == Cesium3DTilesSelection::TilesetLoadType::TilesetJson
				? CesiumLoadFailure::Stage::TilesetJson
				: CesiumLoadFailure::Stage::StageUnknown;
		record.message = failData.message;
		record.httpStatusCode = static_cast<int32_t>(failData.statusCode);
		failureQueue->push(std::move(record));
	};
	this->m_tilesetConfig->options.tileLoadErrorCallback = [failureQueue](
		const Cesium3DTilesSelection::TileLoadFailureDetails& details
	) {
		if (failureQueue == nullptr) {
			return;
		}
		CesiumLoadFailureRecord record;
		record.category = CesiumLoadFailure::Category::TileContent;
		record.stage = CesiumLoadFailure::Stage::TileContentRequest;
		record.message = details.message;
		if (details.pTile != nullptr) {
			record.tileId =
				Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
					details.pTile->getTileID()
				);
		}
		if (details.pRequest != nullptr) {
			record.url = redact_cesium_diagnostic_url(details.pRequest->url());
			if (details.pRequest->response() != nullptr) {
				record.httpStatusCode = static_cast<int32_t>(
					details.pRequest->response()->statusCode()
				);
			}
		}
		const int32_t status = record.httpStatusCode;
		record.retryable = status == 0 || status == 408 || status == 425 ||
			status == 429 || status == 500 || status == 502 || status == 503 ||
			status == 504 || (status >= 520 && status <= 527);
		failureQueue->push(std::move(record));
	};

	Cesium3DTilesContent::registerAllTileContentTypes();
}


Cesium3DTileset::~Cesium3DTileset() {
	// Release Cesium while the Godot-facing receiver and the rest of this adapter
	// are still valid. IPrepareRendererResources::free may synchronously deliver
	// the final unloading callbacks during this reset.
	this->reset_movement_prediction();
	this->cancel_height_requests(
		"Tileset was destroyed before height sampling completed."
	);
	this->disconnect_metadata_style();
	this->disconnect_point_cloud_shading();
	this->release_active_tileset();
	this->m_requestCacheDatabase.reset();
	delete this->m_tilesetConfig;
	this->m_tilesetConfig = nullptr;
}

void Cesium3DTileset::set_maximum_screen_space_error(real_t error)
{
	this->m_tilesetConfig->options.maximumScreenSpaceError = error;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().maximumScreenSpaceError = error;
	}
	this->refresh_point_cloud_shading();
}

real_t Cesium3DTileset::get_maximum_screen_space_error() const
{
	return this->m_tilesetConfig->options.maximumScreenSpaceError;
}

void Cesium3DTileset::set_maximum_simultaneous_tile_loads(uint32_t count)
{
	this->m_tilesetConfig->options.maximumSimultaneousTileLoads = count;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().maximumSimultaneousTileLoads = count;
	}
}

uint32_t Cesium3DTileset::get_maximum_simultaneous_tile_loads() const
{
	return this->m_tilesetConfig->options.maximumSimultaneousTileLoads;
}

void Cesium3DTileset::set_stale_request_cancellation_enabled(bool enabled) {
	this->m_tilesetConfig->options.enableStaleRequestCancellation = enabled;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().enableStaleRequestCancellation =
			enabled;
	}
}

bool Cesium3DTileset::get_stale_request_cancellation_enabled() const {
	return this->m_tilesetConfig->options.enableStaleRequestCancellation;
}

void Cesium3DTileset::set_preload_ancestors(bool preload)
{
	this->m_tilesetConfig->options.preloadAncestors = preload;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().preloadAncestors = preload;
	}
}

bool Cesium3DTileset::get_preload_ancestors() const
{
	return this->m_tilesetConfig->options.preloadAncestors;
}

void Cesium3DTileset::set_preload_siblings(bool preload)
{
	this->m_tilesetConfig->options.preloadSiblings = preload;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().preloadSiblings = preload;
	}
}

bool Cesium3DTileset::get_preload_siblings() const
{
	return this->m_tilesetConfig->options.preloadSiblings;
}

void Cesium3DTileset::set_loading_descendant_limit(uint32_t limit)
{
	this->m_tilesetConfig->options.loadingDescendantLimit = limit;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().loadingDescendantLimit = limit;
	}
}

uint32_t Cesium3DTileset::get_loading_descendant_limit() const
{
	return this->m_tilesetConfig->options.loadingDescendantLimit;
}

void Cesium3DTileset::set_forbid_holes(bool forbidHoles)
{
	this->m_tilesetConfig->options.forbidHoles = forbidHoles;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().forbidHoles = forbidHoles;
	}
}

bool Cesium3DTileset::get_forbid_holes() const
{
	return this->m_tilesetConfig->options.forbidHoles;
}

void Cesium3DTileset::set_frustum_culling_enabled(bool enabled) {
	this->m_tilesetConfig->options.enableFrustumCulling = enabled;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().enableFrustumCulling = enabled;
	}
}

bool Cesium3DTileset::get_frustum_culling_enabled() const {
	return this->m_tilesetConfig->options.enableFrustumCulling;
}

void Cesium3DTileset::set_fog_culling_enabled(bool enabled) {
	this->m_tilesetConfig->options.enableFogCulling = enabled;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().enableFogCulling = enabled;
	}
}

bool Cesium3DTileset::get_fog_culling_enabled() const {
	return this->m_tilesetConfig->options.enableFogCulling;
}

void Cesium3DTileset::set_enforce_culled_screen_space_error(bool enforce) {
	this->m_tilesetConfig->options.enforceCulledScreenSpaceError = enforce;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().enforceCulledScreenSpaceError =
			enforce;
	}
}

bool Cesium3DTileset::get_enforce_culled_screen_space_error() const {
	return this->m_tilesetConfig->options.enforceCulledScreenSpaceError;
}

void Cesium3DTileset::set_culled_screen_space_error(real_t error) {
	const double boundedError = std::max(0.0, static_cast<double>(error));
	this->m_tilesetConfig->options.culledScreenSpaceError = boundedError;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().culledScreenSpaceError =
			boundedError;
	}
}

real_t Cesium3DTileset::get_culled_screen_space_error() const {
	return static_cast<real_t>(
		this->m_tilesetConfig->options.culledScreenSpaceError
	);
}

void Cesium3DTileset::set_render_tiles_under_camera(bool enabled) {
	this->m_tilesetConfig->options.renderTilesUnderCamera = enabled;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().renderTilesUnderCamera = enabled;
	}
}

bool Cesium3DTileset::get_render_tiles_under_camera() const {
	return this->m_tilesetConfig->options.renderTilesUnderCamera;
}

void Cesium3DTileset::set_lod_transitions_enabled(bool enabled) {
	if (
		this->m_tilesetConfig->options.enableLodTransitionPeriod == enabled
	) {
		return;
	}
	this->m_tilesetConfig->options.enableLodTransitionPeriod = enabled;
	// Fade-capable shaders contain a fragment discard. Recreate only when the
	// opt-in changes so ordinary tilesets never pay that shader cost.
	this->recreate_tileset();
}

bool Cesium3DTileset::get_lod_transitions_enabled() const {
	return this->m_tilesetConfig->options.enableLodTransitionPeriod;
}

void Cesium3DTileset::set_lod_transition_length(real_t seconds) {
	const float boundedSeconds = static_cast<float>(
		std::max<real_t>(0.001, seconds)
	);
	this->m_tilesetConfig->options.lodTransitionLength = boundedSeconds;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().lodTransitionLength =
			boundedSeconds;
	}
}

real_t Cesium3DTileset::get_lod_transition_length() const {
	return static_cast<real_t>(
		this->m_tilesetConfig->options.lodTransitionLength
	);
}

void Cesium3DTileset::set_kick_descendants_while_fading_in(bool enabled) {
	this->m_tilesetConfig->options.kickDescendantsWhileFadingIn = enabled;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().kickDescendantsWhileFadingIn =
			enabled;
	}
}

bool Cesium3DTileset::get_kick_descendants_while_fading_in() const {
	return this->m_tilesetConfig->options.kickDescendantsWhileFadingIn;
}

void Cesium3DTileset::set_translucency_sort_priority(int32_t priority) {
	const int32_t boundedPriority = std::clamp(priority, -128, 127);
	if (this->m_translucencySortPriority == boundedPriority) {
		return;
	}
	this->m_translucencySortPriority = boundedPriority;
	this->refresh_translucency_sort_priority();
}

int32_t Cesium3DTileset::get_translucency_sort_priority() const {
	return this->m_translucencySortPriority;
}

void Cesium3DTileset::set_translucency_depth_prepass_enabled(bool enabled) {
	if (this->m_translucencyDepthPrepassEnabled == enabled) {
		return;
	}
	this->m_translucencyDepthPrepassEnabled = enabled;
	this->recreate_tileset();
}

bool Cesium3DTileset::get_translucency_depth_prepass_enabled() const {
	return this->m_translucencyDepthPrepassEnabled;
}

void Cesium3DTileset::set_maximum_cached_bytes(int64_t bytes) {
	const int64_t boundedBytes = std::max<int64_t>(0, bytes);
	this->m_automaticHardwareBudgetsEnabled = false;
	this->m_cacheBudgetSource = "explicit";
	this->m_tilesetConfig->options.maximumCachedBytes = boundedBytes;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().maximumCachedBytes = boundedBytes;
	}
}

int64_t Cesium3DTileset::get_maximum_cached_bytes() const {
	return this->m_tilesetConfig->options.maximumCachedBytes;
}

void Cesium3DTileset::set_automatic_hardware_budgets_enabled(bool enabled) {
	if (this->m_automaticHardwareBudgetsEnabled == enabled) {
		return;
	}
	this->m_automaticHardwareBudgetsEnabled = enabled;
	if (enabled) {
		this->recalculate_automatic_hardware_budgets();
	} else if (this->m_cacheBudgetSource == String("automatic_hardware")) {
		this->m_cacheBudgetSource = "automatic_frozen";
	}
}

bool Cesium3DTileset::get_automatic_hardware_budgets_enabled() const {
	return this->m_automaticHardwareBudgetsEnabled;
}

void Cesium3DTileset::set_hardware_budget_profile(int32_t profile) {
	const CesiumHardwareBudgetProfile boundedProfile =
		static_cast<CesiumHardwareBudgetProfile>(
			std::clamp(profile, 0, 2)
		);
	if (this->m_hardwareBudgetProfile == boundedProfile) {
		return;
	}
	this->m_hardwareBudgetProfile = boundedProfile;
	if (this->m_automaticHardwareBudgetsEnabled) {
		this->recalculate_automatic_hardware_budgets();
	}
}

int32_t Cesium3DTileset::get_hardware_budget_profile() const {
	return static_cast<int32_t>(this->m_hardwareBudgetProfile);
}

void Cesium3DTileset::set_automatic_cache_budget_share(real_t share) {
	const real_t boundedShare = std::clamp<real_t>(share, 0.0, 1.0);
	if (Math::is_equal_approx(this->m_automaticCacheBudgetShare, boundedShare)) {
		return;
	}
	this->m_automaticCacheBudgetShare = boundedShare;
	if (this->m_automaticHardwareBudgetsEnabled) {
		this->apply_automatic_cache_budget();
	}
}

real_t Cesium3DTileset::get_automatic_cache_budget_share() const {
	return this->m_automaticCacheBudgetShare;
}

int64_t Cesium3DTileset::recalculate_automatic_hardware_budgets() {
	this->capture_hardware_capabilities();
	if (this->m_automaticHardwareBudgetsEnabled) {
		this->apply_automatic_cache_budget();
	}
	return this->get_maximum_cached_bytes();
}

Dictionary Cesium3DTileset::get_hardware_capabilities() {
	if (!this->m_hardwareCapabilitiesCaptured) {
		this->capture_hardware_capabilities();
	}
	return this->m_hardwareCapabilities.to_dictionary();
}

int64_t Cesium3DTileset::get_recommended_total_cache_bytes(int32_t profile) {
	if (!this->m_hardwareCapabilitiesCaptured) {
		this->capture_hardware_capabilities();
	}
	return this->m_hardwareCapabilities.recommend_total_cache_bytes(
		static_cast<CesiumHardwareBudgetProfile>(std::clamp(profile, 0, 2))
	);
}

void Cesium3DTileset::capture_hardware_capabilities() {
	this->m_hardwareCapabilities = CesiumHardwareCapabilities::capture();
	this->m_hardwareCapabilitiesCaptured = true;
}

void Cesium3DTileset::apply_automatic_cache_budget() {
	const int64_t totalRecommendation =
		this->m_hardwareCapabilities.recommend_total_cache_bytes(
			this->m_hardwareBudgetProfile
		);
	const int64_t tilesetRecommendation = static_cast<int64_t>(std::floor(
		static_cast<double>(totalRecommendation) *
		static_cast<double>(this->m_automaticCacheBudgetShare)
	));
	this->m_tilesetConfig->options.maximumCachedBytes = tilesetRecommendation;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().maximumCachedBytes =
			tilesetRecommendation;
	}
	this->m_cacheBudgetSource = "automatic_hardware";
}

void Cesium3DTileset::set_worker_thread_count(uint32_t count) {
	const uint32_t boundedCount = std::clamp<uint32_t>(count, 1, 64);
	this->m_workerThreadCountAutomatic = false;
	if (this->m_workerThreadCount == boundedCount) {
		return;
	}
	this->m_workerThreadCount = boundedCount;
	this->recreate_tileset();
}

uint32_t Cesium3DTileset::get_worker_thread_count() const {
	return this->m_workerThreadCount;
}

void Cesium3DTileset::reset_worker_thread_count_to_automatic() {
	const uint32_t hardwareThreads = static_cast<uint32_t>(std::max(
		1,
		this->m_hardwareCapabilitiesCaptured
			? this->m_hardwareCapabilities.logicalProcessorCount
			: static_cast<int32_t>(std::thread::hardware_concurrency())
	));
	const uint32_t recommended = std::clamp<uint32_t>(
		hardwareThreads > 1 ? hardwareThreads - 1 : 1,
		1,
		8
	);
	const bool changed = this->m_workerThreadCount != recommended;
	this->m_workerThreadCount = recommended;
	this->m_workerThreadCountAutomatic = true;
	if (changed) {
		this->recreate_tileset();
	}
}

void Cesium3DTileset::set_main_thread_loading_time_limit_ms(
	real_t milliseconds
) {
	const real_t boundedMilliseconds = std::max<real_t>(0.0, milliseconds);
	this->m_tilesetConfig->options.mainThreadLoadingTimeLimit =
		boundedMilliseconds;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().mainThreadLoadingTimeLimit =
			boundedMilliseconds;
	}
}

real_t Cesium3DTileset::get_main_thread_loading_time_limit_ms() const {
	return this->m_tilesetConfig->options.mainThreadLoadingTimeLimit;
}

void Cesium3DTileset::set_maximum_primitive_geometry_upload_bytes(
	int64_t bytes
) {
	const int64_t boundedBytes = std::max<int64_t>(0, bytes);
	if (this->m_maximumPrimitiveGeometryUploadBytes == boundedBytes) {
		return;
	}
	this->m_maximumPrimitiveGeometryUploadBytes = boundedBytes;
	this->recreate_tileset();
}

int64_t Cesium3DTileset::get_maximum_primitive_geometry_upload_bytes() const {
	return this->m_maximumPrimitiveGeometryUploadBytes;
}

void Cesium3DTileset::set_maximum_primitive_texture_upload_bytes(
	int64_t bytes
) {
	const int64_t boundedBytes = std::max<int64_t>(0, bytes);
	if (this->m_maximumPrimitiveTextureUploadBytes == boundedBytes) {
		return;
	}
	this->m_maximumPrimitiveTextureUploadBytes = boundedBytes;
	this->recreate_tileset();
}

int64_t Cesium3DTileset::get_maximum_primitive_texture_upload_bytes() const {
	return this->m_maximumPrimitiveTextureUploadBytes;
}

void Cesium3DTileset::set_tile_cache_unload_time_limit_ms(
	real_t milliseconds
) {
	const real_t boundedMilliseconds = std::max<real_t>(0.0, milliseconds);
	this->m_tilesetConfig->options.tileCacheUnloadTimeLimit =
		boundedMilliseconds;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions().tileCacheUnloadTimeLimit =
			boundedMilliseconds;
	}
}

real_t Cesium3DTileset::get_tile_cache_unload_time_limit_ms() const {
	return this->m_tilesetConfig->options.tileCacheUnloadTimeLimit;
}

void Cesium3DTileset::set_http_cache_enabled(bool enabled) {
	if (this->m_httpCacheEnabled == enabled) {
		return;
	}
	this->m_httpCacheEnabled = enabled;
	this->recreate_tileset();
}

bool Cesium3DTileset::get_http_cache_enabled() const {
	return this->m_httpCacheEnabled;
}

void Cesium3DTileset::set_http_cache_path(const String& path) {
	if (this->m_httpCachePath == path) {
		return;
	}
	this->m_httpCachePath = path;
	this->recreate_tileset();
}

const String& Cesium3DTileset::get_http_cache_path() const {
	return this->m_httpCachePath;
}

String Cesium3DTileset::get_resolved_http_cache_path() const {
	return this->m_resolvedHttpCachePath.is_empty()
		? resolve_cache_path(this->m_httpCachePath)
		: this->m_resolvedHttpCachePath;
}

void Cesium3DTileset::set_http_cache_maximum_items(int64_t maximumItems) {
	const uint64_t boundedItems = static_cast<uint64_t>(
		std::max<int64_t>(0, maximumItems)
	);
	if (this->m_httpCacheMaximumItems == boundedItems) {
		return;
	}
	this->m_httpCacheMaximumItems = boundedItems;
	this->recreate_tileset();
}

int64_t Cesium3DTileset::get_http_cache_maximum_items() const {
	return static_cast<int64_t>(std::min<uint64_t>(
		this->m_httpCacheMaximumItems,
		static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
	));
}

void Cesium3DTileset::set_http_cache_maximum_data_bytes(
	int64_t maximumBytes
) {
	const uint64_t boundedBytes = static_cast<uint64_t>(
		std::max<int64_t>(0, maximumBytes)
	);
	if (this->m_httpCacheMaximumDataBytes == boundedBytes) {
		return;
	}
	this->m_httpCacheMaximumDataBytes = boundedBytes;
	this->recreate_tileset();
}

int64_t Cesium3DTileset::get_http_cache_maximum_data_bytes() const {
	return static_cast<int64_t>(std::min<uint64_t>(
		this->m_httpCacheMaximumDataBytes,
		static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
	));
}

void Cesium3DTileset::set_http_cache_prune_interval_requests(
	int32_t requests
) {
	const int32_t boundedRequests = std::max<int32_t>(1, requests);
	if (this->m_httpCachePruneIntervalRequests == boundedRequests) {
		return;
	}
	this->m_httpCachePruneIntervalRequests = boundedRequests;
	this->recreate_tileset();
}

int32_t Cesium3DTileset::get_http_cache_prune_interval_requests() const {
	return this->m_httpCachePruneIntervalRequests;
}

bool Cesium3DTileset::clear_http_cache() {
	if (this->m_requestCacheDatabase != nullptr) {
		return this->m_requestCacheDatabase->clearAll();
	}

	const String resolvedPath = this->get_resolved_http_cache_path();
	if (resolvedPath.is_empty()) {
		return false;
	}
	const Error directoryError = DirAccess::make_dir_recursive_absolute(
		resolvedPath.get_base_dir()
	);
	if (directoryError != Error::OK) {
		ERR_PRINT(
			"Could not create HTTP cache directory: " +
			resolvedPath.get_base_dir()
		);
		return false;
	}
	try {
		auto cache = std::make_shared<CesiumAsync::SqliteCache>(
			spdlog::default_logger(),
			resolvedPath.utf8().get_data(),
			this->m_httpCacheMaximumItems,
			this->m_httpCacheMaximumDataBytes
		);
		return cache->clearAll();
	} catch (const std::exception& exception) {
		ERR_PRINT(
			"Could not clear HTTP cache " + resolvedPath + ": " +
			exception.what()
		);
		return false;
	}
}

bool Cesium3DTileset::prune_http_cache() {
	if (this->m_requestCacheDatabase != nullptr) {
		return this->m_requestCacheDatabase->prune();
	}

	const String resolvedPath = this->get_resolved_http_cache_path();
	if (resolvedPath.is_empty()) {
		return false;
	}
	const Error directoryError = DirAccess::make_dir_recursive_absolute(
		resolvedPath.get_base_dir()
	);
	if (directoryError != Error::OK) {
		return false;
	}
	try {
		auto cache = std::make_shared<CesiumAsync::SqliteCache>(
			spdlog::default_logger(),
			resolvedPath.utf8().get_data(),
			this->m_httpCacheMaximumItems,
			this->m_httpCacheMaximumDataBytes
		);
		return cache->prune();
	} catch (const std::exception& exception) {
		ERR_PRINT(
			"Could not prune HTTP cache " + resolvedPath + ": " +
			exception.what()
		);
		return false;
	}
}

void Cesium3DTileset::set_maximum_network_retries(int32_t retries) {
	const int32_t boundedRetries = std::clamp<int32_t>(retries, 0, 16);
	if (this->m_maximumNetworkRetries == boundedRetries) {
		return;
	}
	this->m_maximumNetworkRetries = boundedRetries;
	this->recreate_tileset();
}

int32_t Cesium3DTileset::get_maximum_network_retries() const {
	return this->m_maximumNetworkRetries;
}

void Cesium3DTileset::set_network_retry_initial_delay_seconds(
	real_t seconds
) {
	const real_t bounded = std::clamp<real_t>(seconds, 0.0, 60.0);
	if (Math::is_equal_approx(
		this->m_networkRetryInitialDelaySeconds,
		bounded
	)) {
		return;
	}
	this->m_networkRetryInitialDelaySeconds = bounded;
	if (this->m_networkRetryMaximumDelaySeconds < bounded) {
		this->m_networkRetryMaximumDelaySeconds = bounded;
	}
	this->recreate_tileset();
}

real_t Cesium3DTileset::get_network_retry_initial_delay_seconds() const {
	return this->m_networkRetryInitialDelaySeconds;
}

void Cesium3DTileset::set_network_retry_maximum_delay_seconds(real_t seconds) {
	const real_t bounded = std::clamp<real_t>(
		seconds,
		this->m_networkRetryInitialDelaySeconds,
		300.0
	);
	if (Math::is_equal_approx(
		this->m_networkRetryMaximumDelaySeconds,
		bounded
	)) {
		return;
	}
	this->m_networkRetryMaximumDelaySeconds = bounded;
	this->recreate_tileset();
}

real_t Cesium3DTileset::get_network_retry_maximum_delay_seconds() const {
	return this->m_networkRetryMaximumDelaySeconds;
}

void Cesium3DTileset::set_movement_prediction_enabled(bool enabled) {
	if (this->m_movementPredictionEnabled == enabled) {
		return;
	}
	this->m_movementPredictionEnabled = enabled;
	this->reset_movement_prediction();
}

bool Cesium3DTileset::get_movement_prediction_enabled() const {
	return this->m_movementPredictionEnabled;
}

void Cesium3DTileset::set_movement_prediction_seconds(real_t seconds) {
	this->m_movementPredictionSeconds = std::max<real_t>(0.0, seconds);
}

real_t Cesium3DTileset::get_movement_prediction_seconds() const {
	return this->m_movementPredictionSeconds;
}

void Cesium3DTileset::set_movement_prediction_minimum_speed(
	real_t metersPerSecond
) {
	this->m_movementPredictionMinimumSpeed =
		std::max<real_t>(0.0, metersPerSecond);
}

real_t Cesium3DTileset::get_movement_prediction_minimum_speed() const {
	return this->m_movementPredictionMinimumSpeed;
}

void Cesium3DTileset::set_movement_prediction_maximum_distance(
	real_t meters
) {
	this->m_movementPredictionMaximumDistance = std::max<real_t>(0.0, meters);
}

real_t Cesium3DTileset::get_movement_prediction_maximum_distance() const {
	return this->m_movementPredictionMaximumDistance;
}

void Cesium3DTileset::set_movement_prediction_weight(real_t weight) {
	this->m_movementPredictionWeight = std::clamp<real_t>(weight, 0.01, 100.0);
	if (this->m_predictionViewGroup != nullptr) {
		this->m_predictionViewGroup->setWeight(
			this->m_movementPredictionWeight
		);
	}
}

real_t Cesium3DTileset::get_movement_prediction_weight() const {
	return this->m_movementPredictionWeight;
}

void Cesium3DTileset::set_turn_prediction_enabled(bool enabled) {
	this->m_turnPredictionEnabled = enabled;
}

bool Cesium3DTileset::get_turn_prediction_enabled() const {
	return this->m_turnPredictionEnabled;
}

void Cesium3DTileset::set_turn_prediction_minimum_angular_speed_degrees(
	real_t degreesPerSecond
) {
	this->m_turnPredictionMinimumAngularSpeedDegrees =
		std::max<real_t>(0.0, degreesPerSecond);
}

real_t Cesium3DTileset::get_turn_prediction_minimum_angular_speed_degrees()
	const {
	return this->m_turnPredictionMinimumAngularSpeedDegrees;
}

void Cesium3DTileset::set_turn_prediction_maximum_angle_degrees(
	real_t degrees
) {
	this->m_turnPredictionMaximumAngleDegrees = std::clamp<real_t>(
		degrees,
		0.0,
		180.0
	);
}

real_t Cesium3DTileset::get_turn_prediction_maximum_angle_degrees() const {
	return this->m_turnPredictionMaximumAngleDegrees;
}

void Cesium3DTileset::set_zoom_out_prediction_enabled(bool enabled) {
	this->m_zoomOutPredictionEnabled = enabled;
}

bool Cesium3DTileset::get_zoom_out_prediction_enabled() const {
	return this->m_zoomOutPredictionEnabled;
}

void Cesium3DTileset::set_zoom_out_prediction_minimum_rate(real_t rate) {
	this->m_zoomOutPredictionMinimumRate = std::max<real_t>(0.0, rate);
}

real_t Cesium3DTileset::get_zoom_out_prediction_minimum_rate() const {
	return this->m_zoomOutPredictionMinimumRate;
}

void Cesium3DTileset::set_zoom_out_prediction_maximum_scale(real_t scale) {
	this->m_zoomOutPredictionMaximumScale = std::clamp<real_t>(
		scale,
		1.0,
		16.0
	);
}

real_t Cesium3DTileset::get_zoom_out_prediction_maximum_scale() const {
	return this->m_zoomOutPredictionMaximumScale;
}

void Cesium3DTileset::set_url(const String& url)
{
	if (this->m_url == url) {
		return;
	}
	this->m_url = url;
	this->recreate_tileset();
}

const String& Cesium3DTileset::get_url() const
{
	return this->m_url;
}

void Cesium3DTileset::set_request_headers(const Dictionary& headers) {
	// Normalize into a new Dictionary so neither the setter argument nor a
	// subsequently returned Dictionary can mutate the active configuration.
	const Dictionary normalized = normalize_request_headers(headers);
	if (request_headers_equal(this->m_requestHeaders, normalized)) {
		return;
	}
	this->m_requestHeaders = normalized.duplicate(true);
	this->m_tilesetConfig->options.requestHeaders =
		request_headers_to_native(this->m_requestHeaders);
	this->recreate_tileset();
}

Dictionary Cesium3DTileset::get_request_headers() const {
	return this->m_requestHeaders.duplicate(true);
}

void Cesium3DTileset::set_credit(const String& credit) {
	const CharString utf8 = credit.utf8();
	const std::optional<std::string> next = credit.is_empty()
		? std::nullopt
		: std::make_optional(std::string(utf8.get_data(), utf8.length()));
	if (this->m_tilesetConfig->options.credit == next) {
		return;
	}
	this->m_tilesetConfig->options.credit = next;
	this->recreate_tileset();
}

String Cesium3DTileset::get_credit() const {
	const std::optional<std::string>& credit =
		this->m_tilesetConfig->options.credit;
	return credit.has_value() ? String::utf8(credit->c_str()) : String();
}

void Cesium3DTileset::set_credit_system(
	CesiumGDCreditSystem* creditSystem
) {
	const ObjectID nextId = creditSystem == nullptr
		? ObjectID()
		: ObjectID(creditSystem->get_instance_id());
	if (this->m_configuredCreditSystem == nextId) {
		return;
	}
	this->m_configuredCreditSystem = nextId;
	this->invalidate_resolved_credit_system();
}

CesiumGDCreditSystem* Cesium3DTileset::get_credit_system() const {
	if (this->m_configuredCreditSystem == ObjectID()) {
		return nullptr;
	}
	return Object::cast_to<CesiumGDCreditSystem>(
		ObjectDB::get_instance(this->m_configuredCreditSystem)
	);
}

CesiumGDCreditSystem* Cesium3DTileset::resolve_credit_system() {
	if (this->m_resolvedCreditSystem != ObjectID()) {
		CesiumGDCreditSystem* resolved =
			Object::cast_to<CesiumGDCreditSystem>(
				ObjectDB::get_instance(this->m_resolvedCreditSystem)
			);
		if (resolved != nullptr) {
			return resolved;
		}
		this->m_resolvedCreditSystem = ObjectID();
	}

	CesiumGDCreditSystem* result = this->get_credit_system();
	if (result == nullptr && this->is_inside_tree()) {
		result = CesiumGDCreditSystem::get_singleton(this);
	}
	if (result != nullptr) {
		this->m_resolvedCreditSystem = ObjectID(result->get_instance_id());
	}
	return result;
}

CesiumGDCreditSystem* Cesium3DTileset::get_resolved_credit_system() const {
	if (this->m_resolvedCreditSystem == ObjectID()) {
		return nullptr;
	}
	return Object::cast_to<CesiumGDCreditSystem>(
		ObjectDB::get_instance(this->m_resolvedCreditSystem)
	);
}

void Cesium3DTileset::invalidate_resolved_credit_system() {
	this->m_resolvedCreditSystem = ObjectID();
	this->recreate_tileset();
}

void Cesium3DTileset::set_show_credits_on_screen(bool showOnScreen) {
	if (this->m_tilesetConfig->options.showCreditsOnScreen == showOnScreen) {
		return;
	}
	this->m_tilesetConfig->options.showCreditsOnScreen = showOnScreen;
	// Existing glTF content credits capture this flag when their renderer
	// preparation completes. Recreate so already-loaded content and future
	// content use one consistent presentation policy.
	this->recreate_tileset();
}

bool Cesium3DTileset::get_show_credits_on_screen() const {
	return this->m_tilesetConfig->options.showCreditsOnScreen;
}

void Cesium3DTileset::set_generate_missing_normals_smooth(bool shouldGenerate)
{
	this->m_tilesetConfig->options.contentOptions.generateMissingNormalsSmooth =
		shouldGenerate;
	if (this->m_activeTileset != nullptr) {
		this->m_activeTileset->getOptions()
			.contentOptions.generateMissingNormalsSmooth = shouldGenerate;
	}
}


bool Cesium3DTileset::get_generate_missing_normals_smooth() const
{
	return this->m_tilesetConfig->options.contentOptions.generateMissingNormalsSmooth;
}

int Cesium3DTileset::get_data_source() const
{
	return static_cast<int>(this->m_selectedDataSource);
}

void Cesium3DTileset::set_data_source(int data_source)
{
	const CesiumDataSource selected = static_cast<CesiumDataSource>(data_source);
	if (this->m_selectedDataSource == selected) {
		return;
	}
	this->m_selectedDataSource = selected;
	this->notify_property_list_changed();
	this->recreate_tileset();
}

void Cesium3DTileset::set_ion_asset_id(int64_t id)
{
	if (this->m_cesiumIonAssetId == id) {
		return;
	}
	this->m_cesiumIonAssetId = id;
	this->recreate_tileset();
}

int64_t Cesium3DTileset::get_ion_asset_id() const
{
	return this->m_cesiumIonAssetId;
}

void Cesium3DTileset::set_create_physics_meshes(bool shouldCreate)
{
	this->m_createPhysicsMeshes = shouldCreate;
}

bool Cesium3DTileset::get_create_physics_meshes() const
{
	return this->m_createPhysicsMeshes;
}

void Cesium3DTileset::set_camera_manager_path(
	const NodePath& cameraManagerPath
) {
	if (this->m_cameraManagerPath == cameraManagerPath) {
		return;
	}
	this->m_cameraManagerPath = cameraManagerPath;
	this->reset_movement_prediction();
}

NodePath Cesium3DTileset::get_camera_manager_path() const {
	return this->m_cameraManagerPath;
}

void Cesium3DTileset::update_tileset(const Transform3D& cameraTransform)
{
	if (this->m_networkAssetAccessor != nullptr) {
		this->m_networkAssetAccessor->tick();
	}
	this->schedule_load_failure_dispatches();
	
	this->is_georeferenced(&this->m_georeference);
	if (this->m_activeTileset == nullptr) {
		if (this->m_georeference != nullptr) {
			this->m_georeference->set_should_update_origin(true);
			this->m_georeference->register_tileset_to_move_origin(this);
		}
		
		this->load_tileset();
	}
	if (this->m_activeTileset == nullptr) {
		this->m_lastViewStateValid = false;
		this->m_lastSelectionViewCount = 0;
		this->m_lastSelectionRenderViewCount = 0;
		return;
	}

	// Cesium Native 0.59+ no longer dispatches main-thread continuations from
	// updateViewGroup or loadTiles. Pump this tileset's AsyncSystem exactly once
	// before its per-frame selection and loading work.
	this->m_activeTileset->getAsyncSystem().dispatchMainThreadTasks();

	// Cesium Native accepts all render cameras in one view-group update. With no
	// manager configured, retain the historical current-viewport-camera path.
	Viewport* currentViewport = this->get_viewport();
	Camera3D* viewportCamera = currentViewport == nullptr
		? nullptr
		: currentViewport->get_camera_3d();
	std::vector<Camera3D*> selectionCameras;
	this->m_lastSelectionInvalidCameraCount = 0;
	this->m_lastSelectionWrongWorldCameraCount = 0;
	this->m_lastSelectionDuplicateCameraCount = 0;
	this->m_lastSelectionCameraManagerConfigured =
		!this->m_cameraManagerPath.is_empty();
	this->m_lastSelectionCameraManagerResolved = false;
	bool cameraManagerControlsSelection = false;
	if (this->m_lastSelectionCameraManagerConfigured) {
		Node* managerNode = this->get_node_or_null(this->m_cameraManagerPath);
		CesiumCameraManager* manager =
			Object::cast_to<CesiumCameraManager>(managerNode);
		if (manager != nullptr) {
			this->m_lastSelectionCameraManagerResolved = true;
			cameraManagerControlsSelection = true;
			const CesiumCameraManager::Resolution resolution =
				manager->resolve_cameras(this);
			selectionCameras = resolution.cameras;
			this->m_lastSelectionInvalidCameraCount =
				resolution.invalid_count();
			this->m_lastSelectionWrongWorldCameraCount =
				resolution.wrongWorld;
			this->m_lastSelectionDuplicateCameraCount =
				resolution.duplicates;
		} else {
			// A stale manager path should be visible in diagnostics without making
			// an otherwise healthy player viewport disappear.
			++this->m_lastSelectionInvalidCameraCount;
			if (viewportCamera != nullptr) {
				selectionCameras.push_back(viewportCamera);
			}
		}
	} else if (viewportCamera != nullptr) {
		selectionCameras.push_back(viewportCamera);
	}

	std::vector<Cesium3DTilesSelection::ViewState> renderViewStates;
	renderViewStates.reserve(selectionCameras.size());
	std::optional<CesiumGodotCameraProjection> primaryProjection;
	Camera3D* primaryCamera = nullptr;
	glm::dvec3 camPos{0.0};
	glm::dvec3 cameraDirection{0.0, 0.0, -1.0};
	glm::dvec3 cameraUpNative{0.0, 1.0, 0.0};
	for (Camera3D* camera : selectionCameras) {
		if (camera == nullptr) {
			++this->m_lastSelectionInvalidCameraCount;
			continue;
		}
		Viewport* cameraViewport = camera->get_viewport();
		if (cameraViewport == nullptr) {
			++this->m_lastSelectionInvalidCameraCount;
			continue;
		}
		Vector2 viewportSize;
		#if defined(CESIUM_GD_EXT)
		viewportSize = cameraViewport->get_visible_rect().get_size();
		#elif defined(CESIUM_GD_MODULE)
		viewportSize = cameraViewport->get_camera_rect_size();
		#endif
		const std::optional<CesiumGodotCameraProjection> projection =
			CesiumGodotCameraProjection::from_camera(camera, viewportSize);
		if (!projection.has_value()) {
			++this->m_lastSelectionInvalidCameraCount;
			continue;
		}

		Transform3D selectionCameraTransform;
		if (camera == viewportCamera) {
			selectionCameraTransform = cameraTransform;
			// Preserve the legacy explicit-transform contract while matching the
			// current Camera3D's rendered eye offsets.
			selectionCameraTransform.origin +=
				selectionCameraTransform.basis
					.get_column(Vector3::AXIS_X)
					.normalized() * camera->get_h_offset();
			selectionCameraTransform.origin +=
				selectionCameraTransform.basis
					.get_column(Vector3::AXIS_Y)
					.normalized() * camera->get_v_offset();
		} else {
			// Godot's camera transform includes camera offsets and any camera-side
			// basis normalization needed by an auxiliary render view.
			selectionCameraTransform = camera->get_camera_transform();
		}

		glm::dvec3 position;
		glm::dvec3 direction;
		glm::dvec3 up;
		if (this->m_georeference != nullptr) {
			const Transform3D cameraInGeoreference =
				this->m_georeference->get_global_transform().affine_inverse() *
				selectionCameraTransform;
			position = this->m_georeference->get_coordinate_system()
				.localPositionToEcef(CesiumMathUtils::to_glm_dvec3(
					cameraInGeoreference.origin
				));
			direction = this->m_georeference->get_coordinate_system()
				.localDirectionToEcef(CesiumMathUtils::to_glm_dvec3(
					-cameraInGeoreference.basis.get_column(Vector3::AXIS_Z)
				));
			up = this->m_georeference->get_coordinate_system()
				.localDirectionToEcef(CesiumMathUtils::to_glm_dvec3(
					cameraInGeoreference.basis.get_column(Vector3::AXIS_Y)
				));
		} else {
			position = CesiumMathUtils::to_glm_dvec3(
				selectionCameraTransform.origin
			);
			direction = CesiumMathUtils::to_glm_dvec3(
				-selectionCameraTransform.basis.get_column(Vector3::AXIS_Z)
			);
			up = CesiumMathUtils::to_glm_dvec3(
				selectionCameraTransform.basis.get_column(Vector3::AXIS_Y)
			);
		}
		const double directionLengthSquared = glm::dot(direction, direction);
		const double upLengthSquared = glm::dot(up, up);
		const glm::dvec3 orthogonal = glm::cross(direction, up);
		const double orthogonalLengthSquared = glm::dot(
			orthogonal,
			orthogonal
		);
		if (
			!std::isfinite(position.x) || !std::isfinite(position.y) ||
			!std::isfinite(position.z) ||
			!std::isfinite(directionLengthSquared) ||
			!std::isfinite(upLengthSquared) ||
			!std::isfinite(orthogonalLengthSquared) ||
			directionLengthSquared <= 1.0e-20 || upLengthSquared <= 1.0e-20 ||
			orthogonalLengthSquared <= 1.0e-20
		) {
			++this->m_lastSelectionInvalidCameraCount;
			continue;
		}
		direction = glm::normalize(direction);
		up = glm::normalize(up);
		renderViewStates.emplace_back(projection->create_view_state(
			position,
			direction,
			up
		));
		if (!primaryProjection.has_value()) {
			primaryProjection = projection;
			primaryCamera = camera;
			camPos = position;
			cameraDirection = direction;
			cameraUpNative = up;
		}
	}
	this->m_lastSelectionRenderViewCount = static_cast<int32_t>(
		renderViewStates.size()
	);
	this->m_lastSelectionViewCount = this->m_lastSelectionRenderViewCount;
	this->m_lastViewStateValid = primaryProjection.has_value();
	if (primaryProjection.has_value()) {
		this->m_lastViewProjectionType =
			primaryProjection->get_projection_type();
		this->m_lastViewProjectionTypeName =
			primaryProjection->get_projection_type_name();
		this->m_lastViewKeepWidth = primaryProjection->get_keep_width();
		this->m_lastViewViewportSize = Vector2(
			static_cast<real_t>(primaryProjection->get_viewport_size().x),
			static_cast<real_t>(primaryProjection->get_viewport_size().y)
		);
		this->m_lastViewPosition = camPos;
		this->m_lastViewDirection = cameraDirection;
		this->m_lastViewUp = cameraUpNative;
		this->m_lastViewHorizontalFieldOfViewRadians =
			primaryProjection->get_horizontal_field_of_view_radians();
		this->m_lastViewVerticalFieldOfViewRadians =
			primaryProjection->get_vertical_field_of_view_radians();
		this->m_lastViewPlaneExtents = Vector4(
			static_cast<real_t>(primaryProjection->get_left()),
			static_cast<real_t>(primaryProjection->get_right()),
			static_cast<real_t>(primaryProjection->get_bottom()),
			static_cast<real_t>(primaryProjection->get_top())
		);
		this->m_lastViewNearPlane = primaryProjection->get_near_plane();
	} else {
		this->m_lastViewProjectionType = 0;
		this->m_lastViewProjectionTypeName = "unavailable";
		this->m_lastViewKeepWidth = false;
		this->m_lastViewViewportSize = Vector2();
		this->m_lastViewPosition = glm::dvec3(0.0);
		this->m_lastViewDirection = glm::dvec3(0.0);
		this->m_lastViewUp = glm::dvec3(0.0);
		this->m_lastViewHorizontalFieldOfViewRadians = 0.0;
		this->m_lastViewVerticalFieldOfViewRadians = 0.0;
		this->m_lastViewPlaneExtents = Vector4();
		this->m_lastViewNearPlane = 0.0;
	}
	if (renderViewStates.empty() && !cameraManagerControlsSelection) {
		// Preserve the legacy behavior when a viewport temporarily has no current
		// camera: pause selection without hiding already-realized content. An
		// explicitly resolved manager, by contrast, owns the empty-view decision.
		this->reset_movement_prediction();
		return;
	}

	CesiumCameraPredictionSettings predictionSettings;
	predictionSettings.enabled = this->m_movementPredictionEnabled;
	predictionSettings.horizonSeconds = static_cast<double>(
		this->m_movementPredictionSeconds
	);
	predictionSettings.minimumLinearSpeed = static_cast<double>(
		this->m_movementPredictionMinimumSpeed
	);
	predictionSettings.maximumDistance = static_cast<double>(
		this->m_movementPredictionMaximumDistance
	);
	predictionSettings.turnPredictionEnabled = this->m_turnPredictionEnabled;
	predictionSettings.minimumAngularSpeedRadians =
		static_cast<double>(this->m_turnPredictionMinimumAngularSpeedDegrees) *
		std::numbers::pi / 180.0;
	predictionSettings.maximumAngleRadians =
		static_cast<double>(this->m_turnPredictionMaximumAngleDegrees) *
		std::numbers::pi / 180.0;
	predictionSettings.zoomOutPredictionEnabled =
		this->m_zoomOutPredictionEnabled;
	predictionSettings.minimumZoomOutRate = static_cast<double>(
		this->m_zoomOutPredictionMinimumRate
	);
	predictionSettings.maximumZoomOutScale = static_cast<double>(
		this->m_zoomOutPredictionMaximumScale
	);
	CesiumCameraPrediction prediction;
	if (primaryProjection.has_value() && primaryCamera != nullptr) {
		const ObjectID primaryCameraId(primaryCamera->get_instance_id());
		if (this->m_predictionCameraId != primaryCameraId) {
			this->reset_movement_prediction();
			this->m_predictionCameraId = primaryCameraId;
		}
		prediction = this->m_cameraPredictor.sample(
			camPos,
			cameraDirection,
			cameraUpNative,
			primaryProjection->get_prediction_span(),
			predictionSettings
		);
	} else {
		this->reset_movement_prediction();
	}
	const double frameDeltaSeconds = prediction.deltaSeconds;
	this->m_lastPredictionActive = prediction.active;
	this->m_lastTranslationPredictionActive = prediction.translationActive;
	this->m_lastTurnPredictionActive = prediction.turnActive;
	this->m_lastZoomOutPredictionActive = prediction.zoomOutActive;
	this->m_lastPredictionSpeed = prediction.linearSpeed;
	this->m_lastPredictionDistance = prediction.distance;
	this->m_lastPredictionAngularSpeedDegrees =
		prediction.angularSpeedRadians * 180.0 / std::numbers::pi;
	this->m_lastPredictionAngleDegrees =
		prediction.angleRadians * 180.0 / std::numbers::pi;
	this->m_lastPredictionZoomOutRate = prediction.zoomOutRate;
	this->m_lastPredictionProjectionScale = prediction.projectionScale;
	this->m_lastPredictedViewDirection = prediction.direction;
	this->m_lastPredictionSuppressedByLodTransitions =
		this->m_lastPredictionActive && this->get_lod_transitions_enabled();
	if (this->m_lastPredictionSuppressedByLodTransitions) {
		// Cesium Native stores one fade percentage per tile and explicitly
		// recommends against driving it from multiple TilesetViewGroups. Keep the
		// visible view authoritative while this optional mode is active.
		this->m_lastPredictionActive = false;
	}
	this->m_lastSelectionViewCount =
		this->m_lastSelectionRenderViewCount +
		(this->m_lastPredictionActive ? 1 : 0);

	const auto selectionStart = std::chrono::steady_clock::now();
	const Cesium3DTilesSelection::ViewUpdateResult& updateResult =
		this->m_activeTileset->updateViewGroup(
			this->m_activeTileset->getDefaultViewGroup(),
			renderViewStates,
			static_cast<float>(frameDeltaSeconds)
		);
	if (renderViewStates.empty()) {
		// Native deliberately returns no fading list for an empty view set. Hide
		// realized content explicitly so a manager with every camera disabled does
		// not leave stale visuals or collision active in the Godot scene.
		this->m_activeTileset->forEachLoadedTile(
			[this](const Cesium3DTilesSelection::Tile& tile) {
				this->despawn_tile(tile);
			}
		);
	}
	if (this->m_lastPredictionActive) {
		if (this->m_predictionViewGroup == nullptr) {
			this->m_predictionViewGroup = std::make_unique<
				Cesium3DTilesSelection::TilesetViewGroup
			>();
		}
		this->m_predictionViewGroup->setWeight(
			this->m_movementPredictionWeight
		);
		const Cesium3DTilesSelection::ViewState predictedViewState =
			primaryProjection->create_view_state(
				prediction.position,
				prediction.direction,
				prediction.up,
				prediction.projectionScale
			);
		const Cesium3DTilesSelection::ViewUpdateResult& predictionResult =
			this->m_activeTileset->updateViewGroup(
				*this->m_predictionViewGroup,
				{ predictedViewState },
				static_cast<float>(frameDeltaSeconds)
			);
		this->m_lastPredictionWorkerQueueLength =
			predictionResult.workerThreadTileLoadQueueLength;
		this->m_lastPredictionMainQueueLength =
			predictionResult.mainThreadTileLoadQueueLength;
	} else {
		// Unregister immediately so a stale future view cannot keep requesting
		// work after the camera stops or prediction is disabled.
		this->m_predictionViewGroup.reset();
		this->m_lastPredictionWorkerQueueLength = 0;
		this->m_lastPredictionMainQueueLength = 0;
	}
	this->m_selectionMicroseconds = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - selectionStart
		).count()
	);
	this->m_lastWorkerQueueLength = updateResult.workerThreadTileLoadQueueLength;
	this->m_lastMainQueueLength = updateResult.mainThreadTileLoadQueueLength;
	this->m_lastSelectedTileCount = static_cast<int32_t>(
		updateResult.tilesToRenderThisFrame.size()
	);
	this->m_lastFadingTileCount = static_cast<int32_t>(
		updateResult.tilesFadingOut.size()
	);
	this->m_lastTilesVisited = updateResult.tilesVisited;
	this->m_lastCulledTilesVisited = updateResult.culledTilesVisited;
	this->m_lastTilesCulled = updateResult.tilesCulled;
	this->m_lastTilesOccluded = updateResult.tilesOccluded;
	this->m_lastTilesWaitingForOcclusionResults =
		updateResult.tilesWaitingForOcclusionResults;
	this->m_lastTilesKicked = updateResult.tilesKicked;
	this->m_lastMaximumDepthVisited = updateResult.maxDepthVisited;
	const auto loadTilesStart = std::chrono::steady_clock::now();
	this->m_activeTileset->loadTiles();
	this->m_loadTilesMicroseconds = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - loadTilesStart
		).count()
	);
	this->capture_debug_tile_states(updateResult);
	this->m_lastLodTransitionActiveTileCount = 0;
	this->m_lastLodTransitionSupportedPrimitiveCount = 0;
	this->m_lastLodTransitionUnsupportedPrimitiveCount = 0;
	this->m_lastLodTransitionCompatibleRenderNodeCount = 0;
	this->m_lastLodTransitionMinimumPercentage = 1.0;
	this->m_lastLodTransitionMaximumPercentage = 0.0;
	const bool lodTransitionsEnabled = this->get_lod_transitions_enabled();
	auto applyLodTransition = [this](
		const Cesium3DTilesSelection::Tile& tile,
		bool fadingOut
	) {
		const Cesium3DTilesSelection::TileRenderContent* renderContent =
			tile.getContent().getRenderContent();
		if (renderContent == nullptr) {
			return;
		}
		Cesium3DTile* tileNode = static_cast<Cesium3DTile*>(
			renderContent->getRenderResources()
		);
		if (tileNode == nullptr) {
			return;
		}
		const float percentage = std::clamp(
			renderContent->getLodTransitionFadePercentage(),
			0.0f,
			1.0f
		);
		const CesiumLodTransitionController::MaterialCoverage coverage =
			CesiumLodTransitionController::apply(
				tileNode,
				percentage,
				fadingOut
			);
		this->m_lastLodTransitionSupportedPrimitiveCount +=
			coverage.supportedPrimitives;
		this->m_lastLodTransitionUnsupportedPrimitiveCount +=
			coverage.unsupportedPrimitives;
		this->m_lastLodTransitionCompatibleRenderNodeCount +=
			coverage.compatibleRenderNodes;
		this->m_lastLodTransitionMinimumPercentage = std::min(
			this->m_lastLodTransitionMinimumPercentage,
			static_cast<double>(percentage)
		);
		this->m_lastLodTransitionMaximumPercentage = std::max(
			this->m_lastLodTransitionMaximumPercentage,
			static_cast<double>(percentage)
		);
		if (percentage < 0.99999f) {
			++this->m_lastLodTransitionActiveTileCount;
		}
	};

	for (CesiumUtility::IntrusivePointer<const Cesium3DTilesSelection::Tile> tile : updateResult.tilesToRenderThisFrame) {
		this->render_tile_as_node(*tile);
		if (lodTransitionsEnabled) {
			applyLodTransition(*tile, false);
		}
	}
	for (CesiumUtility::IntrusivePointer<const Cesium3DTilesSelection::Tile> tile : updateResult.tilesFadingOut) {
		if (!lodTransitionsEnabled) {
			despawn_tile(*tile);
			continue;
		}
		const Cesium3DTilesSelection::TileRenderContent* renderContent =
			tile->getContent().getRenderContent();
		if (renderContent == nullptr) {
			continue;
		}
		Cesium3DTile* tileNode = static_cast<Cesium3DTile*>(
			renderContent->getRenderResources()
		);
		if (tileNode != nullptr && this->m_createPhysicsMeshes) {
			// Collision transfers to the replacement immediately. Retaining two
			// visual LODs must not create duplicate physics contacts.
			tileNode->set_tile_collision_enabled(false);
		}
		applyLodTransition(*tile, true);
		if (renderContent->getLodTransitionFadePercentage() >= 1.0f) {
			despawn_tile(*tile);
		}
	}
	if (
		this->m_lastLodTransitionSupportedPrimitiveCount == 0 &&
		this->m_lastLodTransitionUnsupportedPrimitiveCount == 0
	) {
		this->m_lastLodTransitionMinimumPercentage = 1.0;
		this->m_lastLodTransitionMaximumPercentage = 1.0;
	}
}

void Cesium3DTileset::set_debug_boundig_volumes_func(const Callable& onTileDrawn) {
	this->m_debugVolumesFunction = onTileDrawn;
}

bool Cesium3DTileset::is_initial_loading_finished() const
{
	return this->m_initialLoadingFinished;
}

bool Cesium3DTileset::has_active_tileset() const
{
	return this->m_activeTileset != nullptr;
}

void Cesium3DTileset::set_debug_tile_state_capture_enabled(bool enabled) {
	if (this->m_debugTileStateCaptureEnabled == enabled) {
		return;
	}
	this->m_debugTileStateCaptureEnabled = enabled;
	if (!enabled) {
		this->m_debugTileStates.clear();
		this->m_debugTileStatesTruncated = false;
	}
}

bool Cesium3DTileset::get_debug_tile_state_capture_enabled() const {
	return this->m_debugTileStateCaptureEnabled;
}

void Cesium3DTileset::set_debug_tile_state_limit(int32_t limit) {
	this->m_debugTileStateLimit = std::clamp(limit, 1, 4096);
}

int32_t Cesium3DTileset::get_debug_tile_state_limit() const {
	return this->m_debugTileStateLimit;
}

Array Cesium3DTileset::get_debug_tile_states() const {
	return this->m_debugTileStates.duplicate(false);
}

int64_t Cesium3DTileset::get_debug_tile_state_frame_number() const {
	return static_cast<int64_t>(this->m_debugTileStateFrameNumber);
}

bool Cesium3DTileset::get_debug_tile_states_truncated() const {
	return this->m_debugTileStatesTruncated;
}

Transform3D Cesium3DTileset::get_debug_world_bounds_transform(
	const Ref<CesiumBoundingVolume>& bounds
) const {
	if (bounds.is_null() || !bounds->is_valid()) {
		return Transform3D();
	}
	const Transform3D sourceBounds = bounds->get_oriented_box_transform();
	if (this->m_georeference == nullptr) {
		return this->get_global_transform() * sourceBounds;
	}
	return this->m_georeference->get_global_transform() *
		CesiumMathUtils::from_glm_mat4(
			this->m_georeference->ecef_transform_to_local(
				CesiumMathUtils::to_glm_mat4(sourceBounds)
			)
		);
}

void Cesium3DTileset::capture_debug_tile_states(
	const Cesium3DTilesSelection::ViewUpdateResult& updateResult
) {
	if (!this->m_debugTileStateCaptureEnabled) {
		return;
	}
	++this->m_debugTileStateFrameNumber;
	this->m_debugTileStates.clear();
	this->m_debugTileStatesTruncated = false;

	using Tile = Cesium3DTilesSelection::Tile;
	using Role = CesiumTileDebugState::SelectionRole;
	struct Candidate {
		const Tile* tile = nullptr;
		Role role = Role::RoleUnknown;
	};
	std::vector<Candidate> candidates;
	candidates.reserve(static_cast<size_t>(this->m_debugTileStateLimit));
	std::unordered_map<const Tile*, size_t> candidateIndices;
	candidateIndices.reserve(static_cast<size_t>(this->m_debugTileStateLimit));

	auto rolePriority = [](Role role) -> int32_t {
		switch (role) {
		case Role::Selected: return 4;
		case Role::Fading: return 3;
		case Role::Ancestor: return 2;
		case Role::Child: return 1;
		case Role::RoleUnknown:
		default: return 0;
		}
	};
	auto addCandidate = [&](const Tile* tile, Role role) {
		if (tile == nullptr) {
			return;
		}
		const auto existing = candidateIndices.find(tile);
		if (existing != candidateIndices.end()) {
			Candidate& candidate = candidates[existing->second];
			if (rolePriority(role) > rolePriority(candidate.role)) {
				candidate.role = role;
			}
			return;
		}
		if (
			static_cast<int32_t>(candidates.size()) >=
			this->m_debugTileStateLimit
		) {
			this->m_debugTileStatesTruncated = true;
			return;
		}
		candidateIndices.emplace(tile, candidates.size());
		candidates.push_back(Candidate{tile, role});
	};

	for (const auto& tile : updateResult.tilesToRenderThisFrame) {
		addCandidate(tile.get(), Role::Selected);
	}
	for (const auto& tile : updateResult.tilesFadingOut) {
		addCandidate(tile.get(), Role::Fading);
	}
	const size_t directlySelectedCount = candidates.size();
	for (size_t index = 0; index < directlySelectedCount; ++index) {
		const Tile* parent = candidates[index].tile->getParent();
		int32_t guard = 0;
		while (parent != nullptr && guard++ < 1024) {
			addCandidate(parent, Role::Ancestor);
			parent = parent->getParent();
		}
	}
	for (size_t index = 0; index < directlySelectedCount; ++index) {
		for (const Tile& child : candidates[index].tile->getChildren()) {
			addCandidate(&child, Role::Child);
		}
	}

	for (const Candidate& candidate : candidates) {
		const Tile& tile = *candidate.tile;
		const Tile* parent = tile.getParent();
		int32_t depth = 0;
		for (const Tile* ancestor = parent;
			ancestor != nullptr && depth < 1024;
			ancestor = ancestor->getParent()) {
			++depth;
		}
		const std::string tileId =
			Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
				tile.getTileID()
			);
		const std::string parentTileId = parent == nullptr
			? std::string()
			: Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
				parent->getTileID()
			);
		const std::string hierarchyPath = get_tile_hierarchy_path(&tile);
		const std::string parentHierarchyPath =
			get_tile_hierarchy_path(parent);
		Ref<CesiumBoundingVolume> tileBounds;
		tileBounds.instantiate();
		tileBounds->initialize(create_cesium_bounding_volume_snapshot(
			tile.getBoundingVolume()
		));
		Ref<CesiumBoundingVolume> contentBounds;
		if (tile.getContentBoundingVolume()) {
			contentBounds.instantiate();
			contentBounds->initialize(create_cesium_bounding_volume_snapshot(
				*tile.getContentBoundingVolume()
			));
		}
		Ref<CesiumBoundingVolume> viewerRequestBounds;
		if (tile.getViewerRequestVolume()) {
			viewerRequestBounds.instantiate();
			viewerRequestBounds->initialize(
				create_cesium_bounding_volume_snapshot(
					*tile.getViewerRequestVolume()
				)
			);
		}
		Ref<CesiumTileDebugState> state;
		state.instantiate();
		state->initialize(
			this->m_debugTileStateFrameNumber,
			String(tileId.c_str()),
			String(parentTileId.c_str()),
			String(hierarchyPath.c_str()),
			String(parentHierarchyPath.c_str()),
			depth,
			candidate.role,
			static_cast<CesiumTileDebugState::LoadState>(tile.getState()),
			static_cast<CesiumTileDebugState::RefineMode>(tile.getRefine()),
			tile.getGeometricError(),
			static_cast<int32_t>(tile.getChildren().size()),
			tile.getReferenceCount(),
			// Godot does not currently install a Cesium Native GltfModifier, so
			// the modifier-aware v0.63 query receives the matching null value.
			tile.needsWorkerThreadLoading(nullptr),
			tile.needsMainThreadLoading(nullptr),
			tile.isRenderable(),
			tile.isRenderContent(),
			tile.isExternalContent(),
			tile.isEmptyContent(),
			tileBounds,
			contentBounds,
			viewerRequestBounds,
			this->get_debug_world_bounds_transform(tileBounds)
		);
		this->m_debugTileStates.push_back(state);
	}
}

Ref<CesiumBoundingVolume> Cesium3DTileset::get_tileset_bounds() const {
	if (this->m_activeTileset == nullptr) {
		return Ref<CesiumBoundingVolume>();
	}
	if (this->m_tilesetBounds.is_valid()) {
		return this->m_tilesetBounds;
	}
	const Cesium3DTilesSelection::Tile* rootTile =
		this->m_activeTileset->getRootTile();
	if (rootTile == nullptr) {
		return Ref<CesiumBoundingVolume>();
	}
	this->m_tilesetBounds.instantiate();
	this->m_tilesetBounds->initialize(create_cesium_bounding_volume_snapshot(
		rootTile->getBoundingVolume()
	));
	return this->m_tilesetBounds;
}

AABB Cesium3DTileset::get_tileset_source_aabb() const {
	const Ref<CesiumBoundingVolume> bounds = this->get_tileset_bounds();
	return bounds.is_valid() ? bounds->get_source_aabb() : AABB();
}

Ref<CesiumSampleHeightMostDetailedRequest>
Cesium3DTileset::sample_height_most_detailed(
	const PackedVector3Array& longitudeLatitudeHeight
) {
	std::vector<CesiumGeospatial::Cartographic> positions;
	positions.reserve(longitudeLatitudeHeight.size());
	for (int64_t index = 0; index < longitudeLatitudeHeight.size(); ++index) {
		const Vector3& position = longitudeLatitudeHeight[index];
		positions.emplace_back(CesiumGeospatial::Cartographic::fromDegrees(
			static_cast<double>(position.x),
			static_cast<double>(position.y),
			static_cast<double>(position.z)
		));
	}
	return this->start_height_query(std::move(positions));
}

Ref<CesiumSampleHeightMostDetailedRequest>
Cesium3DTileset::sample_height_most_detailed_exact(
	const PackedFloat64Array& longitudeLatitudeHeightComponents
) {
	if (longitudeLatitudeHeightComponents.size() % 3 != 0) {
		ERR_PRINT(
			"sample_height_most_detailed_exact requires flattened "
			"longitude/latitude/height triples"
		);
		return Ref<CesiumSampleHeightMostDetailedRequest>();
	}

	std::vector<CesiumGeospatial::Cartographic> positions;
	positions.reserve(longitudeLatitudeHeightComponents.size() / 3);
	for (
		int64_t index = 0;
		index < longitudeLatitudeHeightComponents.size();
		index += 3
	) {
		const double longitude = longitudeLatitudeHeightComponents[index];
		const double latitude = longitudeLatitudeHeightComponents[index + 1];
		const double height = longitudeLatitudeHeightComponents[index + 2];
		if (
			!std::isfinite(longitude) || !std::isfinite(latitude) ||
			!std::isfinite(height)
		) {
			ERR_PRINT(
				"sample_height_most_detailed_exact requires finite values"
			);
			return Ref<CesiumSampleHeightMostDetailedRequest>();
		}
		positions.emplace_back(CesiumGeospatial::Cartographic::fromDegrees(
			longitude,
			latitude,
			height
		));
	}
	return this->start_height_query(std::move(positions));
}

Ref<CesiumSampleHeightMostDetailedRequest>
Cesium3DTileset::start_height_query(
	std::vector<CesiumGeospatial::Cartographic>&& positions
) {
	if (this->m_activeTileset == nullptr) {
		this->load_tileset();
	}
	if (this->m_activeTileset == nullptr) {
		return Ref<CesiumSampleHeightMostDetailedRequest>();
	}

	Ref<CesiumSampleHeightMostDetailedRequest> request;
	request.instantiate();
	request->initialize(ObjectID(this->get_instance_id()));
	const ObjectID requestId(request->get_instance_id());
	const ObjectID tilesetId(this->get_instance_id());
	this->m_activeHeightRequests.emplace_back(request);

	this->m_activeTileset->sampleHeightMostDetailed(positions)
		.catchImmediately([
			positions = std::move(positions)
		](std::exception&& exception) mutable {
			std::vector<bool> sampleSuccess(positions.size(), false);
			return Cesium3DTilesSelection::SampleHeightResult{
				std::move(positions),
				std::move(sampleSuccess),
				{exception.what()}
			};
		})
		.thenInMainThread([
			requestId,
			tilesetId
		](Cesium3DTilesSelection::SampleHeightResult&& result) {
			CesiumSampleHeightMostDetailedRequest* requestObject =
				Object::cast_to<CesiumSampleHeightMostDetailedRequest>(
					ObjectDB::get_instance(requestId)
				);

			if (requestObject != nullptr && !requestObject->is_finished()) {
				result.sampleSuccess.resize(result.positions.size(), false);
				Array godotResults;
				godotResults.resize(
					static_cast<int64_t>(result.positions.size())
				);
				for (size_t index = 0; index < result.positions.size(); ++index) {
					const CesiumGeospatial::Cartographic& position =
						result.positions[index];
					Ref<CesiumSampleHeightResult> sample;
					sample.instantiate();
					sample->initialize(
						CesiumUtility::Math::radiansToDegrees(position.longitude),
						CesiumUtility::Math::radiansToDegrees(position.latitude),
						position.height,
						result.sampleSuccess[index]
					);
					godotResults[static_cast<int64_t>(index)] = sample;
				}

				PackedStringArray warnings;
				warnings.resize(static_cast<int64_t>(result.warnings.size()));
				for (size_t index = 0; index < result.warnings.size(); ++index) {
					warnings.set(
						static_cast<int64_t>(index),
						String::utf8(result.warnings[index].c_str())
					);
				}
				requestObject->complete(godotResults, warnings);
			}

			Cesium3DTileset* tilesetObject = Object::cast_to<Cesium3DTileset>(
				ObjectDB::get_instance(tilesetId)
			);
			if (tilesetObject != nullptr) {
				tilesetObject->forget_height_request(requestId);
			}
		});

	return request;
}

void Cesium3DTileset::forget_height_request(const ObjectID& requestId) {
	this->m_activeHeightRequests.erase(std::remove_if(
		this->m_activeHeightRequests.begin(),
		this->m_activeHeightRequests.end(),
		[&requestId](
			const Ref<CesiumSampleHeightMostDetailedRequest>& request
		) {
			return request.is_null() ||
				ObjectID(request->get_instance_id()) == requestId;
		}
	), this->m_activeHeightRequests.end());
}

void Cesium3DTileset::cancel_height_requests(const String& warning) {
	for (
		const Ref<CesiumSampleHeightMostDetailedRequest>& request :
		this->m_activeHeightRequests
	) {
		if (request.is_valid()) {
			request->cancel_from_tileset(warning);
		}
	}
	this->m_activeHeightRequests.clear();
}

Dictionary Cesium3DTileset::get_streaming_statistics() const {
	Dictionary result;
	result["active"] = this->m_activeTileset != nullptr;
	result["worker_queue"] = this->m_lastWorkerQueueLength;
	result["main_thread_queue"] = this->m_lastMainQueueLength;
	result["selected"] = this->m_lastSelectedTileCount;
	result["fading_out"] = this->m_lastFadingTileCount;
	result["tiles_visited"] = static_cast<int64_t>(this->m_lastTilesVisited);
	result["culled_tiles_visited"] = static_cast<int64_t>(
		this->m_lastCulledTilesVisited
	);
	result["tiles_culled"] = static_cast<int64_t>(this->m_lastTilesCulled);
	result["tiles_occluded"] = static_cast<int64_t>(
		this->m_lastTilesOccluded
	);
	result["tiles_waiting_for_occlusion_results"] = static_cast<int64_t>(
		this->m_lastTilesWaitingForOcclusionResults
	);
	result["occlusion_culling_available"] = false;
	result["occlusion_culling_enabled"] = false;
	result["occlusion_culling_backend"] = "unavailable";
	result["occlusion_culling_unavailable_reason"] =
		OCCLUSION_CULLING_UNAVAILABLE_REASON;
	result["tiles_kicked"] = static_cast<int64_t>(this->m_lastTilesKicked);
	result["maximum_depth_visited"] = static_cast<int64_t>(
		this->m_lastMaximumDepthVisited
	);
	result["selection_us"] = static_cast<int64_t>(this->m_selectionMicroseconds);
	result["load_tiles_us"] = static_cast<int64_t>(this->m_loadTilesMicroseconds);
	result["debug_tile_state_capture_enabled"] =
		this->m_debugTileStateCaptureEnabled;
	result["debug_tile_state_limit"] = this->m_debugTileStateLimit;
	result["debug_tile_state_count"] = this->m_debugTileStates.size();
	result["debug_tile_state_frame"] = static_cast<int64_t>(
		this->m_debugTileStateFrameNumber
	);
	result["debug_tile_states_truncated"] =
		this->m_debugTileStatesTruncated;
	result["failed"] = static_cast<int64_t>(this->m_terminalTileFailureCount);
	result["cache_limit_bytes"] = this->get_maximum_cached_bytes();
	result["cache_budget_source"] = this->m_cacheBudgetSource;
	result["automatic_hardware_budgets_enabled"] =
		this->m_automaticHardwareBudgetsEnabled;
	result["hardware_budget_profile"] = static_cast<int32_t>(
		this->m_hardwareBudgetProfile
	);
	result["hardware_budget_profile_name"] =
		this->m_hardwareBudgetProfile ==
			CesiumHardwareBudgetProfile::Conservative
		? "conservative"
		: this->m_hardwareBudgetProfile ==
				CesiumHardwareBudgetProfile::Aggressive
			? "aggressive"
			: "balanced";
	result["automatic_cache_budget_share"] =
		this->m_automaticCacheBudgetShare;
	result["hardware_capabilities"] =
		this->m_hardwareCapabilities.to_dictionary();
	result["maximum_simultaneous_tile_loads"] = static_cast<int64_t>(
		this->get_maximum_simultaneous_tile_loads()
	);
	result["maximum_screen_space_error"] =
		this->get_maximum_screen_space_error();
	result["preload_ancestors"] = this->get_preload_ancestors();
	result["preload_siblings"] = this->get_preload_siblings();
	result["loading_descendant_limit"] = static_cast<int64_t>(
		this->get_loading_descendant_limit()
	);
	result["forbid_holes"] = this->get_forbid_holes();
	result["frustum_culling_enabled"] =
		this->get_frustum_culling_enabled();
	result["fog_culling_enabled"] = this->get_fog_culling_enabled();
	result["enforce_culled_screen_space_error"] =
		this->get_enforce_culled_screen_space_error();
	result["culled_screen_space_error"] =
		this->get_culled_screen_space_error();
	result["render_tiles_under_camera"] =
		this->get_render_tiles_under_camera();
	result["lod_transitions_enabled"] = this->get_lod_transitions_enabled();
	result["lod_transition_length"] = this->get_lod_transition_length();
	result["kick_descendants_while_fading_in"] =
		this->get_kick_descendants_while_fading_in();
	result["lod_transition_active_tiles"] =
		this->m_lastLodTransitionActiveTileCount;
	result["lod_transition_supported_primitives"] =
		this->m_lastLodTransitionSupportedPrimitiveCount;
	result["lod_transition_unsupported_primitives"] =
		this->m_lastLodTransitionUnsupportedPrimitiveCount;
	result["lod_transition_compatible_render_nodes"] =
		this->m_lastLodTransitionCompatibleRenderNodeCount;
	result["lod_transition_minimum_percentage"] =
		this->m_lastLodTransitionMinimumPercentage;
	result["lod_transition_maximum_percentage"] =
		this->m_lastLodTransitionMaximumPercentage;
	result["view_state_valid"] = this->m_lastViewStateValid;
	result["view_projection_type"] = this->m_lastViewProjectionType;
	result["view_projection_type_name"] =
		this->m_lastViewProjectionTypeName;
	result["view_keep_width"] = this->m_lastViewKeepWidth;
	result["view_viewport_size"] = this->m_lastViewViewportSize;
	result["view_position_components"] =
		vector_components(this->m_lastViewPosition);
	result["view_direction_components"] =
		vector_components(this->m_lastViewDirection);
	result["view_up_components"] = vector_components(this->m_lastViewUp);
	result["view_horizontal_fov_radians"] =
		this->m_lastViewHorizontalFieldOfViewRadians;
	result["view_vertical_fov_radians"] =
		this->m_lastViewVerticalFieldOfViewRadians;
	result["view_plane_extents"] = this->m_lastViewPlaneExtents;
	result["view_near_plane"] = this->m_lastViewNearPlane;
	result["selection_view_count"] = this->m_lastSelectionViewCount;
	result["selection_render_view_count"] =
		this->m_lastSelectionRenderViewCount;
	result["selection_invalid_camera_count"] =
		this->m_lastSelectionInvalidCameraCount;
	result["selection_wrong_world_camera_count"] =
		this->m_lastSelectionWrongWorldCameraCount;
	result["selection_duplicate_camera_count"] =
		this->m_lastSelectionDuplicateCameraCount;
	result["selection_camera_manager_configured"] =
		this->m_lastSelectionCameraManagerConfigured;
	result["selection_camera_manager_resolved"] =
		this->m_lastSelectionCameraManagerResolved;
	result["main_thread_budget_ms"] =
		this->get_main_thread_loading_time_limit_ms();
	result["maximum_primitive_geometry_upload_bytes"] =
		this->m_maximumPrimitiveGeometryUploadBytes;
	result["maximum_primitive_texture_upload_bytes"] =
		this->m_maximumPrimitiveTextureUploadBytes;
	result["cache_unload_budget_ms"] =
		this->get_tile_cache_unload_time_limit_ms();
	result["worker_threads"] = static_cast<int64_t>(this->m_workerThreadCount);
	result["worker_thread_count_source"] =
		this->m_workerThreadCountAutomatic ? "automatic_cpu" : "explicit";
	result["stale_request_cancellation_enabled"] =
		this->get_stale_request_cancellation_enabled();
	result["canceled_tile_loads"] = this->m_activeTileset != nullptr
		? static_cast<int64_t>(
			this->m_activeTileset->getNumberOfCanceledTileLoads()
		)
		: 0;
	result["movement_prediction_enabled"] =
		this->m_movementPredictionEnabled;
	result["movement_prediction_active"] = this->m_lastPredictionActive;
	result["movement_prediction_seconds"] =
		this->m_movementPredictionSeconds;
	result["movement_prediction_minimum_speed"] =
		this->m_movementPredictionMinimumSpeed;
	result["movement_prediction_maximum_distance"] =
		this->m_movementPredictionMaximumDistance;
	result["movement_prediction_weight"] =
		this->m_movementPredictionWeight;
	result["movement_prediction_translation_active"] =
		this->m_lastTranslationPredictionActive;
	result["movement_prediction_speed"] = this->m_lastPredictionSpeed;
	result["movement_prediction_distance"] = this->m_lastPredictionDistance;
	result["turn_prediction_enabled"] = this->m_turnPredictionEnabled;
	result["turn_prediction_active"] = this->m_lastTurnPredictionActive;
	result["turn_prediction_minimum_angular_speed_degrees"] =
		this->m_turnPredictionMinimumAngularSpeedDegrees;
	result["turn_prediction_maximum_angle_degrees"] =
		this->m_turnPredictionMaximumAngleDegrees;
	result["turn_prediction_angular_speed_degrees"] =
		this->m_lastPredictionAngularSpeedDegrees;
	result["turn_prediction_angle_degrees"] =
		this->m_lastPredictionAngleDegrees;
	result["zoom_out_prediction_enabled"] =
		this->m_zoomOutPredictionEnabled;
	result["zoom_out_prediction_active"] =
		this->m_lastZoomOutPredictionActive;
	result["zoom_out_prediction_minimum_rate"] =
		this->m_zoomOutPredictionMinimumRate;
	result["zoom_out_prediction_maximum_scale"] =
		this->m_zoomOutPredictionMaximumScale;
	result["zoom_out_prediction_rate"] =
		this->m_lastPredictionZoomOutRate;
	result["zoom_out_prediction_projection_scale"] =
		this->m_lastPredictionProjectionScale;
	result["predicted_view_direction_components"] =
		vector_components(this->m_lastPredictedViewDirection);
	result["movement_prediction_worker_queue"] =
		this->m_lastPredictionWorkerQueueLength;
	result["movement_prediction_main_thread_queue"] =
		this->m_lastPredictionMainQueueLength;
	result["movement_prediction_suppressed_by_lod_transitions"] =
		this->m_lastPredictionSuppressedByLodTransitions;
	result["http_cache_enabled"] = this->m_httpCacheEnabled;
	result["http_cache_path"] = this->m_httpCachePath;
	const String resolvedCachePath = this->get_resolved_http_cache_path();
	result["http_cache_resolved_path"] = resolvedCachePath;
	result["http_cache_maximum_items"] =
		this->get_http_cache_maximum_items();
	result["http_cache_maximum_data_bytes"] =
		this->get_http_cache_maximum_data_bytes();
	result["http_cache_prune_interval_requests"] =
		this->m_httpCachePruneIntervalRequests;
	result["http_cache_residency_state"] = !this->m_httpCacheEnabled
		? "disabled"
		: this->m_requestCacheDatabase != nullptr
			? "ready"
			: this->m_activeTileset != nullptr ? "unavailable" : "not_initialized";
	result["network_maximum_retries"] = this->m_maximumNetworkRetries;
	result["network_retry_initial_delay_seconds"] =
		this->m_networkRetryInitialDelaySeconds;
	result["network_retry_maximum_delay_seconds"] =
		this->m_networkRetryMaximumDelaySeconds;
	result["load_failure_queue_pending"] = this->m_loadFailureQueue != nullptr
		? static_cast<int64_t>(this->m_loadFailureQueue->size())
		: 0;
	result["load_failure_queue_dropped"] = this->m_loadFailureQueue != nullptr
		? static_cast<int64_t>(this->m_loadFailureQueue->get_dropped_count())
		: 0;
	result["http_cache_disk_bytes"] = static_cast<int64_t>(std::min<uint64_t>(
		sqlite_files_size(resolvedCachePath),
		static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
	));

	int64_t realized = 0;
	int64_t visible = 0;
	for (int32_t index = 0; index < this->get_child_count(); ++index) {
		const auto* tile = Object::cast_to<Cesium3DTile>(this->get_child(index));
		if (tile == nullptr) {
			continue;
		}
		++realized;
		if (tile->is_visible()) {
			++visible;
		}
	}
	result["realized"] = realized;
	result["visible"] = visible;
	result["hidden"] = realized - visible;

	if (this->m_activeTileset != nullptr) {
		result["loaded"] = this->m_activeTileset->getNumberOfTilesLoaded();
		result["loaded_data_bytes"] =
			this->m_activeTileset->getTotalDataBytes();
		result["load_progress_percent"] =
			this->m_activeTileset->computeLoadProgress();
	} else {
		result["loaded"] = 0;
		result["loaded_data_bytes"] = 0;
		result["load_progress_percent"] = 0.0;
	}

	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics =
		this->m_runtimeStatistics;
	if (statistics != nullptr) {
		const uint64_t requestCount = statistics->requestCount.load(
			std::memory_order_relaxed
		);
		const uint64_t requestMicroseconds =
			statistics->requestMicroseconds.load(std::memory_order_relaxed);
		result["requests"] = static_cast<int64_t>(requestCount);
		result["request_attempts"] = static_cast<int64_t>(
			statistics->requestAttemptCount.load(std::memory_order_relaxed)
		);
		result["request_successes"] = static_cast<int64_t>(
			statistics->requestSuccessCount.load(std::memory_order_relaxed)
		);
		result["request_failures"] = static_cast<int64_t>(
			statistics->requestFailureCount.load(std::memory_order_relaxed)
		);
		result["request_cancellations"] = static_cast<int64_t>(
			statistics->requestCancellationCount.load(std::memory_order_relaxed)
		);
		result["request_retries"] = static_cast<int64_t>(
			statistics->requestRetryCount.load(std::memory_order_relaxed)
		);
		result["request_retries_exhausted"] = static_cast<int64_t>(
			statistics->requestRetryExhaustedCount.load(
				std::memory_order_relaxed
			)
		);
		result["requests_in_flight"] = static_cast<int64_t>(
			statistics->requestInFlightCount.load(std::memory_order_relaxed)
		);
		result["requests_maximum_in_flight"] = static_cast<int64_t>(
			statistics->maximumRequestInFlightCount.load(
				std::memory_order_relaxed
			)
		);
		result["request_retries_queued"] = static_cast<int64_t>(
			statistics->requestRetryQueuedCount.load(std::memory_order_relaxed)
		);
		result["request_retries_maximum_queued"] = static_cast<int64_t>(
			statistics->maximumRequestRetryQueuedCount.load(
				std::memory_order_relaxed
			)
		);
		result["request_total_us"] = static_cast<int64_t>(requestMicroseconds);
		result["request_average_us"] = requestCount > 0
			? static_cast<double>(requestMicroseconds) / requestCount
			: 0.0;
		result["request_maximum_us"] = static_cast<int64_t>(
			statistics->requestMaximumMicroseconds.load(
				std::memory_order_relaxed
			)
		);
		result["request_response_bytes"] = static_cast<int64_t>(
			statistics->requestResponseBytes.load(std::memory_order_relaxed)
		);
		const uint64_t decodeCount = statistics->decodeCount.load(
			std::memory_order_relaxed
		);
		const uint64_t decodeMicroseconds = statistics->decodeMicroseconds.load(
			std::memory_order_relaxed
		);
		result["decode_count"] = static_cast<int64_t>(decodeCount);
		result["decode_failures"] = static_cast<int64_t>(
			statistics->decodeFailureCount.load(std::memory_order_relaxed)
		);
		result["decode_total_us"] = static_cast<int64_t>(decodeMicroseconds);
		result["decode_average_us"] = decodeCount > 0
			? static_cast<double>(decodeMicroseconds) / decodeCount
			: 0.0;
		result["decode_maximum_us"] = static_cast<int64_t>(
			statistics->decodeMaximumMicroseconds.load(std::memory_order_relaxed)
		);
		const uint64_t workerCount = statistics->workerPreparationCount.load(
			std::memory_order_relaxed
		);
		const uint64_t realizationCount =
			statistics->mainThreadRealizationCount.load(
				std::memory_order_relaxed
			);
		const uint64_t workerMicroseconds =
			statistics->workerPreparationMicroseconds.load(
				std::memory_order_relaxed
			);
		const uint64_t realizationMicroseconds =
			statistics->mainThreadRealizationMicroseconds.load(
				std::memory_order_relaxed
			);
		result["worker_preparation_count"] = static_cast<int64_t>(workerCount);
		result["worker_preparation_total_us"] =
			static_cast<int64_t>(workerMicroseconds);
		result["worker_preparation_average_us"] = workerCount > 0
			? static_cast<double>(workerMicroseconds) / workerCount
			: 0.0;
		result["worker_preparation_maximum_us"] = static_cast<int64_t>(
			statistics->workerPreparationMaximumMicroseconds.load(
				std::memory_order_relaxed
			)
		);
		result["main_thread_realization_count"] =
			static_cast<int64_t>(realizationCount);
		result["main_thread_realization_total_us"] =
			static_cast<int64_t>(realizationMicroseconds);
		result["main_thread_realization_average_us"] = realizationCount > 0
			? static_cast<double>(realizationMicroseconds) / realizationCount
			: 0.0;
		result["main_thread_realization_maximum_us"] = static_cast<int64_t>(
			statistics->mainThreadRealizationMaximumMicroseconds.load(
				std::memory_order_relaxed
			)
		);
		const uint64_t realizationStepCount =
			statistics->mainThreadRealizationStepCount.load(
				std::memory_order_relaxed
			);
		const uint64_t realizationStepMicroseconds =
			statistics->mainThreadRealizationStepMicroseconds.load(
				std::memory_order_relaxed
			);
		result["main_thread_realization_step_count"] =
			static_cast<int64_t>(realizationStepCount);
		result["main_thread_realization_step_total_us"] =
			static_cast<int64_t>(realizationStepMicroseconds);
		result["main_thread_realization_step_average_us"] =
			realizationStepCount > 0
				? static_cast<double>(realizationStepMicroseconds) /
					realizationStepCount
				: 0.0;
		result["main_thread_realization_step_maximum_us"] =
			static_cast<int64_t>(
				statistics->mainThreadRealizationStepMaximumMicroseconds.load(
					std::memory_order_relaxed
				)
			);
		result["incremental_realization_yields"] = static_cast<int64_t>(
			statistics->incrementalRealizationYieldCount.load(
				std::memory_order_relaxed
			)
		);
		result["oversized_payload_rejections"] = static_cast<int64_t>(
			statistics->oversizedPayloadRejectionCount.load(
				std::memory_order_relaxed
			)
		);
		result["prepared_geometry_total_bytes"] = static_cast<int64_t>(
			statistics->preparedGeometryBytes.load(std::memory_order_relaxed)
		);
		result["prepared_texture_total_bytes"] = static_cast<int64_t>(
			statistics->preparedTextureBytes.load(std::memory_order_relaxed)
		);
		result["prepared_geometry_maximum_tile_bytes"] = static_cast<int64_t>(
			statistics->maximumPreparedGeometryBytes.load(
				std::memory_order_relaxed
			)
		);
		result["prepared_texture_maximum_tile_bytes"] = static_cast<int64_t>(
			statistics->maximumPreparedTextureBytes.load(
				std::memory_order_relaxed
			)
		);
		result["prepared_geometry_maximum_primitive_bytes"] =
			static_cast<int64_t>(
				statistics->maximumPreparedPrimitiveGeometryBytes.load(
					std::memory_order_relaxed
				)
			);
		result["prepared_texture_maximum_primitive_bytes"] =
			static_cast<int64_t>(
				statistics->maximumPreparedPrimitiveTextureBytes.load(
					std::memory_order_relaxed
				)
			);
		result["shared_texture_cache_hits"] = static_cast<int64_t>(
			statistics->sharedTextureCacheHitCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_texture_cache_misses"] = static_cast<int64_t>(
			statistics->sharedTextureCacheMissCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_texture_live_count"] = static_cast<int64_t>(
			statistics->liveSharedTextureCount.load(std::memory_order_relaxed)
		);
		result["shared_texture_live_bytes"] = static_cast<int64_t>(
			statistics->liveSharedTextureBytes.load(std::memory_order_relaxed)
		);
		result["shared_texture_maximum_live_count"] = static_cast<int64_t>(
			statistics->maximumLiveSharedTextureCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_texture_maximum_live_bytes"] = static_cast<int64_t>(
			statistics->maximumLiveSharedTextureBytes.load(
				std::memory_order_relaxed
			)
		);
		result["released_cpu_texture_count"] = static_cast<int64_t>(
			statistics->releasedCpuTextureCount.load(std::memory_order_relaxed)
		);
		result["released_cpu_texture_bytes"] = static_cast<int64_t>(
			statistics->releasedCpuTextureBytes.load(std::memory_order_relaxed)
		);
		result["shared_shader_cache_hits"] = static_cast<int64_t>(
			statistics->sharedShaderCacheHitCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_shader_cache_misses"] = static_cast<int64_t>(
			statistics->sharedShaderCacheMissCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_shader_cache_entries"] = static_cast<int64_t>(
			statistics->liveSharedShaderCount.load(std::memory_order_relaxed)
		);
		result["shared_shader_cache_maximum_entries"] = static_cast<int64_t>(
			statistics->maximumLiveSharedShaderCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_model_cache_hits"] = static_cast<int64_t>(
			statistics->sharedModelCacheHitCount.load(std::memory_order_relaxed)
		);
		result["shared_model_cache_misses"] = static_cast<int64_t>(
			statistics->sharedModelCacheMissCount.load(std::memory_order_relaxed)
		);
		result["shared_model_live_count"] = static_cast<int64_t>(
			statistics->liveSharedModelCount.load(std::memory_order_relaxed)
		);
		result["shared_model_live_geometry_bytes"] = static_cast<int64_t>(
			statistics->liveSharedModelGeometryBytes.load(
				std::memory_order_relaxed
			)
		);
		result["shared_model_live_texture_bytes"] = static_cast<int64_t>(
			statistics->liveSharedModelTextureBytes.load(
				std::memory_order_relaxed
			)
		);
		result["shared_model_maximum_live_count"] = static_cast<int64_t>(
			statistics->maximumLiveSharedModelCount.load(
				std::memory_order_relaxed
			)
		);
		result["shared_model_maximum_live_geometry_bytes"] =
			static_cast<int64_t>(
				statistics->maximumLiveSharedModelGeometryBytes.load(
					std::memory_order_relaxed
				)
			);
		result["shared_model_maximum_live_texture_bytes"] =
			static_cast<int64_t>(
				statistics->maximumLiveSharedModelTextureBytes.load(
					std::memory_order_relaxed
				)
			);
		result["unloaded"] = static_cast<int64_t>(
			statistics->tileUnloadCount.load(std::memory_order_relaxed)
		);
		result["http_cache_lookups"] = static_cast<int64_t>(
			statistics->requestCacheLookupCount.load(std::memory_order_relaxed)
		);
		result["http_cache_hits"] = static_cast<int64_t>(
			statistics->requestCacheHitCount.load(std::memory_order_relaxed)
		);
		result["http_cache_misses"] = static_cast<int64_t>(
			statistics->requestCacheMissCount.load(std::memory_order_relaxed)
		);
		result["http_cache_store_attempts"] = static_cast<int64_t>(
			statistics->requestCacheStoreAttemptCount.load(
				std::memory_order_relaxed
			)
		);
		result["http_cache_store_successes"] = static_cast<int64_t>(
			statistics->requestCacheStoreSuccessCount.load(
				std::memory_order_relaxed
			)
		);
		result["http_cache_stored_payload_bytes"] = static_cast<int64_t>(
			statistics->requestCacheStoredPayloadBytes.load(
				std::memory_order_relaxed
			)
		);
		result["http_cache_prunes"] = static_cast<int64_t>(
			statistics->requestCachePruneCount.load(std::memory_order_relaxed)
		);
		result["http_cache_prune_failures"] = static_cast<int64_t>(
			statistics->requestCachePruneFailureCount.load(
				std::memory_order_relaxed
			)
		);
		const uint64_t cachePruneCount =
			statistics->requestCachePruneCount.load(std::memory_order_relaxed);
		const uint64_t cachePruneMicroseconds =
			statistics->requestCachePruneMicroseconds.load(
				std::memory_order_relaxed
			);
		result["http_cache_prune_total_us"] = static_cast<int64_t>(
			cachePruneMicroseconds
		);
		result["http_cache_prune_average_us"] = cachePruneCount > 0
			? static_cast<double>(cachePruneMicroseconds) / cachePruneCount
			: 0.0;
		result["http_cache_prune_maximum_us"] = static_cast<int64_t>(
			statistics->requestCachePruneMaximumMicroseconds.load(
				std::memory_order_relaxed
			)
		);
		result["http_cache_clears"] = static_cast<int64_t>(
			statistics->requestCacheClearCount.load(std::memory_order_relaxed)
		);
		result["http_cache_clear_failures"] = static_cast<int64_t>(
			statistics->requestCacheClearFailureCount.load(
				std::memory_order_relaxed
			)
		);
	}
	return result;
}

void Cesium3DTileset::add_overlay(CesiumRasterOverlay* overlay)
{
	if (overlay == nullptr || this->m_activeTileset == nullptr) return;
	this->m_activeTileset->getOverlays().add(overlay->get_overlay_instance());
}

void Cesium3DTileset::remove_overlay(CesiumRasterOverlay* overlay)
{
	if (overlay == nullptr || this->m_activeTileset == nullptr) return;
	this->m_activeTileset->getOverlays().remove(overlay->get_overlay_instance());
}

CesiumGeospatial::Ellipsoid
Cesium3DTileset::get_raster_overlay_ellipsoid() const {
	return this->m_activeTileset != nullptr
		? this->m_activeTileset->getOptions().ellipsoid
		: this->m_tilesetConfig->options.ellipsoid;
}

void Cesium3DTileset::add_tile_excluder(
	const std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>& excluder
) {
	if (excluder == nullptr) {
		return;
	}
	auto addUnique = [&excluder](
		std::vector<std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>>& list
	) {
		const auto existing = std::find_if(
			list.begin(),
			list.end(),
			[&excluder](const auto& candidate) {
				return candidate.get() == excluder.get();
			}
		);
		if (existing == list.end()) {
			list.push_back(excluder);
		}
	};
	addUnique(this->m_tilesetConfig->options.excluders);
	if (this->m_activeTileset != nullptr) {
		addUnique(this->m_activeTileset->getOptions().excluders);
	}
}

void Cesium3DTileset::remove_tile_excluder(
	const std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>& excluder
) {
	if (excluder == nullptr) {
		return;
	}
	auto removeMatching = [&excluder](
		std::vector<std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>>& list
	) {
		list.erase(
			std::remove_if(
				list.begin(),
				list.end(),
				[&excluder](const auto& candidate) {
					return candidate.get() == excluder.get();
				}
			),
			list.end()
		);
	};
	removeMatching(this->m_tilesetConfig->options.excluders);
	if (this->m_activeTileset != nullptr) {
		removeMatching(this->m_activeTileset->getOptions().excluders);
	}
}

std::shared_ptr<CesiumLoadFailureQueue>
Cesium3DTileset::get_load_failure_queue() const {
	return this->m_loadFailureQueue;
}

const CesiumAsync::AsyncSystem*
Cesium3DTileset::get_native_async_system() const {
	return this->m_activeTileset == nullptr
		? nullptr
		: &this->m_activeTileset->getAsyncSystem();
}

std::shared_ptr<CesiumAsync::IAssetAccessor>
Cesium3DTileset::get_native_asset_accessor() const {
	return this->m_activeTileset == nullptr
		? std::shared_ptr<CesiumAsync::IAssetAccessor>()
		: this->m_activeTileset->getExternals().pAssetAccessor;
}

void Cesium3DTileset::schedule_load_failure_dispatches() {
	if (this->m_loadFailureQueue == nullptr) {
		return;
	}
	std::vector<CesiumLoadFailureRecord> records =
		this->m_loadFailureQueue->drain();
	const uint64_t tilesetId = static_cast<uint64_t>(this->get_instance_id());
	for (CesiumLoadFailureRecord& record : records) {
		if (record.category == CesiumLoadFailure::Category::TileContent) {
			++this->m_terminalTileFailureCount;
		}
		if (record.sourceInstanceId == 0) {
			record.sourceInstanceId = tilesetId;
		}
		Ref<CesiumLoadFailure> failure;
		failure.instantiate();
		failure->initialize(
			record.sourceInstanceId,
			record.category,
			record.stage,
			String::utf8(record.message.c_str()),
			String::utf8(record.url.c_str()),
			String::utf8(record.tileId.c_str()),
			String::utf8(record.overlayKey.c_str()),
			record.httpStatusCode,
			record.terminal,
			record.retryable,
			record.retryScheduled,
			record.attempt,
			record.maximumAttempts,
			record.retryDelaySeconds
		);

		// Match Cesium for Unreal's deferred delivery: a listener may destroy
		// the source safely because no Cesium Native callback stack is active.
		this->call_deferred("_emit_load_failure_deferred", failure);
		if (record.sourceInstanceId != tilesetId) {
			Object* source = ObjectDB::get_instance(record.sourceInstanceId);
			CesiumRasterOverlay* overlay =
				Object::cast_to<CesiumRasterOverlay>(source);
			if (overlay != nullptr) {
				overlay->call_deferred(
					"_emit_load_failure_deferred",
					failure
				);
			}
		}
	}
}

void Cesium3DTileset::emit_load_failure_deferred(
	const Ref<CesiumLoadFailure>& failure
) {
	this->emit_signal("load_failure", failure);
}

void Cesium3DTileset::free_tile(Cesium3DTile* tileInstance, uint64_t tileHash) {
	if (tileInstance == nullptr) {
		return;
	}
	// Mark for deletion and empty the slot on the hash map
	tileInstance->queue_free();
}

bool Cesium3DTileset::is_georeferenced(CesiumGeoreference** outRef) const
{
	ERR_FAIL_NULL_V(outRef, false);
	if (this->m_georeference != nullptr) {
		*outRef = this->m_georeference;
		return this->m_georeference->get_origin_type() == static_cast<int32_t>(CesiumGeoreference::OriginType::CartographicOrigin);
	}
	//Check if the parent is of type CesiumGDGeoreference
	Node3D* parent = this->get_parent_node_3d();
	*outRef = Object::cast_to<CesiumGeoreference>(parent);
	if (*outRef == nullptr) {
		return false;
	}
	return (*outRef)->get_origin_type() == static_cast<int32_t>(CesiumGeoreference::OriginType::CartographicOrigin);
}


void Cesium3DTileset::move_origin(const glm::dvec3& enginePos) {
	(void)enginePos;
	this->apply_georeference();
}

void Cesium3DTileset::apply_georeference() {
	// Get all tiles
	int32_t childCount = this->get_child_count();
	for(int32_t i = 0; i < childCount; i++) {
		// This applies to user meshes and 3D tiles alike
		GeoreferencedMesh* currMesh = Object::cast_to<GeoreferencedMesh>(this->get_child(i));
		if (currMesh == nullptr) {
			continue;
		}
		currMesh->apply_georeference();
	}
}

void Cesium3DTileset::set_for_each_tile_func(const Callable& onTileFunc) {
	this->m_forEachTileFunction = onTileFunc;
}

void Cesium3DTileset::set_lifecycle_event_receiver(
	Cesium3DTilesetLifecycleEventReceiver* receiver
) {
	this->m_lifecycleEventReceiver =
		receiver == nullptr ? ObjectID() : ObjectID(receiver->get_instance_id());
}

Cesium3DTilesetLifecycleEventReceiver*
Cesium3DTileset::get_lifecycle_event_receiver() const {
	if (this->m_lifecycleEventReceiver.is_null()) {
		return nullptr;
	}
	return Object::cast_to<Cesium3DTilesetLifecycleEventReceiver>(
		ObjectDB::get_instance(this->m_lifecycleEventReceiver)
	);
}

void Cesium3DTileset::disconnect_metadata_style() {
	if (this->m_metadataStyle.is_null()) {
		return;
	}
	const Callable callback(this, "_on_metadata_style_changed");
	if (this->m_metadataStyle->is_connected("changed", callback)) {
		this->m_metadataStyle->disconnect("changed", callback);
	}
}

void Cesium3DTileset::set_metadata_style(
	const Ref<CesiumMetadataStyle>& style
) {
	if (this->m_metadataStyle == style) {
		return;
	}
	if (this->m_metadataStyle.is_valid()) {
		for (int32_t childIndex = 0; childIndex < this->get_child_count(); ++childIndex) {
			Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(
				this->get_child(childIndex)
			);
			if (tile == nullptr) {
				continue;
			}
			const Array primitives = tile->get_loaded_tile_primitives();
			for (int64_t primitiveIndex = 0; primitiveIndex < primitives.size(); ++primitiveIndex) {
				Ref<CesiumLoadedTilePrimitive> primitive = primitives[primitiveIndex];
				this->m_metadataStyle->clear_from_primitive(primitive);
			}
		}
	}
	this->disconnect_metadata_style();
	this->m_metadataStyle = style;
	this->m_metadataStyleEncodingRevision = style.is_valid()
		? style->get_encoding_revision()
		: 0;
	if (style.is_valid()) {
		style->connect(
			"changed",
			Callable(this, "_on_metadata_style_changed")
		);
	}
	this->recreate_tileset();
}

Ref<CesiumMetadataStyle> Cesium3DTileset::get_metadata_style() const {
	return this->m_metadataStyle;
}

void Cesium3DTileset::_on_metadata_style_changed() {
	const uint64_t revision = this->m_metadataStyle.is_valid()
		? this->m_metadataStyle->get_encoding_revision()
		: 0;
	if (revision != this->m_metadataStyleEncodingRevision) {
		this->m_metadataStyleEncodingRevision = revision;
		this->recreate_tileset();
		return;
	}
	this->refresh_metadata_style();
}

void Cesium3DTileset::refresh_metadata_style() {
	for (int32_t childIndex = 0; childIndex < this->get_child_count(); ++childIndex) {
		Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(
			this->get_child(childIndex)
		);
		if (tile == nullptr) {
			continue;
		}
		const Array primitives = tile->get_loaded_tile_primitives();
		for (int64_t primitiveIndex = 0; primitiveIndex < primitives.size(); ++primitiveIndex) {
			Ref<CesiumLoadedTilePrimitive> primitive = primitives[primitiveIndex];
			if (this->m_metadataStyle.is_valid()) {
				this->m_metadataStyle->apply_to_primitive(primitive);
			}
		}
	}
}

void Cesium3DTileset::disconnect_point_cloud_shading() {
	if (this->m_pointCloudShading.is_null()) {
		return;
	}
	const Callable callback(this, "_on_point_cloud_shading_changed");
	if (this->m_pointCloudShading->is_connected("changed", callback)) {
		this->m_pointCloudShading->disconnect("changed", callback);
	}
}

void Cesium3DTileset::set_point_cloud_shading(
	const Ref<CesiumPointCloudShading>& shading
) {
	if (this->m_pointCloudShading == shading && shading.is_valid()) {
		return;
	}
	this->disconnect_point_cloud_shading();
	this->m_pointCloudShading = shading;
	if (this->m_pointCloudShading.is_null()) {
		this->m_pointCloudShading.instantiate();
	}
	this->m_pointCloudShading->connect(
		"changed",
		Callable(this, "_on_point_cloud_shading_changed")
	);
	this->refresh_point_cloud_shading();
}

Ref<CesiumPointCloudShading> Cesium3DTileset::get_point_cloud_shading() const {
	return this->m_pointCloudShading;
}

void Cesium3DTileset::_on_point_cloud_shading_changed() {
	this->refresh_point_cloud_shading();
}

void Cesium3DTileset::refresh_point_cloud_shading() {
	if (this->m_pointCloudShading.is_null()) {
		return;
	}
	for (int32_t childIndex = 0; childIndex < this->get_child_count(); ++childIndex) {
		Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(
			this->get_child(childIndex)
		);
		if (tile == nullptr) {
			continue;
		}
		const Array primitives = tile->get_loaded_tile_primitives();
		for (int64_t primitiveIndex = 0;
			primitiveIndex < primitives.size();
			++primitiveIndex) {
			Ref<CesiumLoadedTilePrimitive> primitive = primitives[primitiveIndex];
			if (primitive.is_null() || !primitive->is_point_primitive()) {
				continue;
			}
			auto apply = [this, &primitive](const Ref<Material>& material) {
				this->m_pointCloudShading->apply_to_material(
					material,
					primitive->get_uses_additive_refinement(),
					primitive->get_tile_geometric_error(),
					primitive->get_primitive_dimensions(),
					primitive->get_point_count(),
					primitive->get_point_diameter(),
					this->get_maximum_screen_space_error()
				);
			};
			apply(primitive->get_default_material());
			apply(primitive->get_selected_base_material());
			apply(primitive->get_active_material());
		}
	}
}

void Cesium3DTileset::refresh_translucency_sort_priority() {
	for (int32_t childIndex = 0;
		childIndex < this->get_child_count();
		++childIndex) {
		Cesium3DTile* tile = Object::cast_to<Cesium3DTile>(
			this->get_child(childIndex)
		);
		if (tile == nullptr) {
			continue;
		}
		const Array primitives = tile->get_loaded_tile_primitives();
		for (int64_t primitiveIndex = 0;
			primitiveIndex < primitives.size();
			++primitiveIndex) {
			Ref<CesiumLoadedTilePrimitive> primitive = primitives[primitiveIndex];
			if (primitive.is_null()) {
				continue;
			}
			for (const Ref<Material>& material : {
				primitive->get_default_material(),
				primitive->get_selected_base_material(),
				primitive->get_active_material()
			}) {
				if (material.is_valid()) {
					material->set_render_priority(
						this->m_translucencySortPriority
					);
				}
			}
		}
	}
}

void Cesium3DTileset::finalize_loaded_tile(
	Cesium3DTile* tile,
	const CesiumGltf::Model& model,
	const CesiumGDPreparedModel& prepared,
	double tileGeometricError,
	bool usesAdditiveRefinement
) {
	ERR_FAIL_NULL(tile);
	Cesium3DTilesetLifecycleEventReceiver* receiver =
		this->get_lifecycle_event_receiver();

	if (tile->get_mesh().is_null()) {
		if (receiver != nullptr) {
			receiver->on_tile_loaded(tile);
		}
		return;
	}

	std::vector<Ref<CesiumLoadedTilePrimitive>> loadedPrimitives;
	std::vector<Ref<Material>> baseSurfaceMaterials;
	loadedPrimitives.reserve(prepared.primitives.size());
	baseSurfaceMaterials.reserve(prepared.primitives.size());

	for (size_t rendererPrimitiveIndex = 0;
		rendererPrimitiveIndex < prepared.primitives.size();
		++rendererPrimitiveIndex) {
		const CesiumGDPreparedPrimitive& preparedPrimitive =
			prepared.primitives[rendererPrimitiveIndex];
		const CesiumGDPrimitiveInstance& instance = preparedPrimitive.source;
		if (
			instance.meshIndex < 0 ||
			instance.meshIndex >= static_cast<int32_t>(model.meshes.size())
		) {
			WARN_PRINT("Cesium primitive instance has an invalid source mesh index");
			continue;
		}
		const CesiumGltf::Mesh& mesh = model.meshes[instance.meshIndex];
		if (
			instance.primitiveIndex < 0 ||
			instance.primitiveIndex >=
				static_cast<int32_t>(mesh.primitives.size())
		) {
			WARN_PRINT("Cesium primitive instance has an invalid primitive index");
			continue;
		}
		const CesiumGltf::MeshPrimitive& primitive =
			mesh.primitives[instance.primitiveIndex];

		Dictionary attributes;
		for (const auto& [semantic, accessorIndex] : primitive.attributes) {
			attributes[String(semantic.c_str())] = accessorIndex;
		}

		const CesiumGltf::Material* gltfMaterial = nullptr;
		if (
			primitive.material >= 0 &&
			primitive.material < static_cast<int32_t>(model.materials.size())
		) {
			gltfMaterial = &model.materials[primitive.material];
		}

		Ref<Material> baseMaterial = preparedPrimitive.realizedMaterial;
		if (baseMaterial.is_valid()) {
			baseMaterial->set_render_priority(this->m_translucencySortPriority);
		}
		if (
			primitive.mode == CesiumGltf::MeshPrimitive::Mode::POINTS &&
			baseMaterial.is_valid() &&
			this->m_pointCloudShading.is_valid()
		) {
			// Tile geometric error and point bounds are placement-specific. Keep
			// immutable shader/texture resources shared, but own the small uniform
			// container per realized point primitive.
			baseMaterial = baseMaterial->duplicate(false);
			this->m_pointCloudShading->apply_to_material(
				baseMaterial,
				usesAdditiveRefinement,
				tileGeometricError,
				preparedPrimitive.dimensions,
				preparedPrimitive.pointCount,
				preparedPrimitive.pointDiameter,
				this->get_maximum_screen_space_error()
			);
			tile->set_primitive_override_material(
				static_cast<int32_t>(rendererPrimitiveIndex),
				baseMaterial
			);
		}

		Ref<CesiumLoadedTilePrimitive> loadedPrimitive;
		loadedPrimitive.instantiate();
		loadedPrimitive->initialize(
			tile,
			tile->get_primitive_render_node(
				static_cast<int32_t>(rendererPrimitiveIndex)
			),
			tile->get_tile_id(),
			static_cast<int32_t>(rendererPrimitiveIndex),
			preparedPrimitive.realizedSurfaceIndex,
			instance.nodeIndex,
			instance.meshIndex,
			instance.primitiveIndex,
			primitive.material,
			primitive.mode,
			preparedPrimitive.pointCount,
			preparedPrimitive.pointDiameter,
			preparedPrimitive.dimensions,
			tileGeometricError,
			usesAdditiveRefinement,
			attributes,
			gltf_material_to_dictionary(gltfMaterial),
			CesiumGDModelLoader::get_texture_coordinate_mappings(primitive),
			baseMaterial,
			tile->get_primitive_features(
				static_cast<int32_t>(rendererPrimitiveIndex)
			),
			preparedPrimitive.realizedInstanceFeatures,
			preparedPrimitive.realizedPrimitiveMetadata,
			tile->get_model_metadata()
		);
		loadedPrimitives.push_back(loadedPrimitive);
		baseSurfaceMaterials.push_back(baseMaterial);
	}
	tile->set_loaded_tile_primitives(loadedPrimitives, baseSurfaceMaterials);

	if (receiver == nullptr) {
		if (this->m_metadataStyle.is_valid()) {
			for (const Ref<CesiumLoadedTilePrimitive>& primitive : loadedPrimitives) {
				this->m_metadataStyle->apply_to_primitive(primitive);
			}
		}
		return;
	}

	for (int32_t index = 0; index < static_cast<int32_t>(loadedPrimitives.size()); ++index) {
		const Ref<CesiumLoadedTilePrimitive>& loadedPrimitive = loadedPrimitives[index];
		Ref<Material> selectedMaterial = receiver->create_material(
			loadedPrimitive,
			baseSurfaceMaterials[index]
		);
		if (selectedMaterial.is_valid()) {
			if (selectedMaterial == baseSurfaceMaterials[index]) {
				// Returning the default is a valid selection, but customization is
				// explicitly per tile. Duplicate only the material parameters; its
				// immutable Shader and Texture resources remain shared.
				selectedMaterial = selectedMaterial->duplicate(false);
			}
			// Per-tile overrides remain safe for both ordinary mesh surfaces and
			// standalone MultiMesh instance components.
			tile->set_primitive_override_material(index, selectedMaterial);
			baseSurfaceMaterials[index] = selectedMaterial;
			loadedPrimitive->set_selected_base_material(selectedMaterial, true);
			loadedPrimitive->apply_world_coordinate_parameters(selectedMaterial);
			receiver->customize_material(loadedPrimitive, selectedMaterial);
			// Like Cesium for Unreal's component-level translucency priority, the
			// tileset policy wins after application customization.
			selectedMaterial->set_render_priority(
				this->m_translucencySortPriority
			);
			if (
				loadedPrimitive->is_point_primitive() &&
				this->m_pointCloudShading.is_valid()
			) {
				this->m_pointCloudShading->apply_to_material(
					selectedMaterial,
					usesAdditiveRefinement,
					tileGeometricError,
					loadedPrimitive->get_primitive_dimensions(),
					loadedPrimitive->get_point_count(),
					loadedPrimitive->get_point_diameter(),
					this->get_maximum_screen_space_error()
				);
			}
		}
		if (this->m_metadataStyle.is_valid()) {
			this->m_metadataStyle->apply_to_primitive(loadedPrimitive);
		}
		receiver->on_tile_mesh_primitive_loaded(loadedPrimitive);
	}
	tile->set_loaded_tile_primitives(loadedPrimitives, baseSurfaceMaterials);
	receiver->on_tile_loaded(tile);
}

void Cesium3DTileset::notify_tile_visibility_changed(
	Cesium3DTile* tile,
	bool visible
) {
	Cesium3DTilesetLifecycleEventReceiver* receiver =
		this->get_lifecycle_event_receiver();
	if (receiver != nullptr && tile != nullptr) {
		receiver->on_tile_visibility_changed(tile, visible);
	}
}

void Cesium3DTileset::notify_tile_unloading(Cesium3DTile* tile) {
	if (tile == nullptr) {
		return;
	}

	Array bindings = tile->take_raster_overlay_bindings();
	for (int32_t index = 0; index < bindings.size(); ++index) {
		Ref<CesiumRasterOverlayBinding> binding = bindings[index];
		this->notify_raster_overlay_detaching(binding);
		binding->clear_from_material(binding->get_material());
		binding->mark_detached();
	}

	Cesium3DTilesetLifecycleEventReceiver* receiver =
		this->get_lifecycle_event_receiver();
	if (receiver != nullptr) {
		receiver->on_tile_unloading(tile);
	}
}

void Cesium3DTileset::notify_raster_overlay_attached(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	Cesium3DTilesetLifecycleEventReceiver* receiver =
		this->get_lifecycle_event_receiver();
	if (receiver != nullptr && binding.is_valid()) {
		receiver->on_raster_overlay_attached(binding);
	}
}

void Cesium3DTileset::notify_raster_overlay_detaching(
	const Ref<CesiumRasterOverlayBinding>& binding
) {
	Cesium3DTilesetLifecycleEventReceiver* receiver =
		this->get_lifecycle_event_receiver();
	if (receiver != nullptr && binding.is_valid()) {
		receiver->on_raster_overlay_detaching(binding);
	}
}

Ref<CesiumRasterOverlayBinding> Cesium3DTileset::attach_raster_overlay(
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	const String& overlayKey,
	const Ref<Texture2D>& texture,
	int32_t textureCoordinateId,
	int32_t textureCoordinateIndex,
	const Vector2& translation,
	const Vector2& scale
) {
	ERR_FAIL_COND_V(primitive.is_null(), Ref<CesiumRasterOverlayBinding>());
	ERR_FAIL_COND_V(texture.is_null(), Ref<CesiumRasterOverlayBinding>());
	ERR_FAIL_COND_V(overlayKey.is_empty(), Ref<CesiumRasterOverlayBinding>());
	Cesium3DTile* tile = primitive->get_loaded_tile();
	ERR_FAIL_NULL_V(tile, Ref<CesiumRasterOverlayBinding>());
	ERR_FAIL_COND_V(
		tile->get_tileset() != this,
		Ref<CesiumRasterOverlayBinding>()
	);

	Ref<CesiumRasterOverlayBinding> binding;
	binding.instantiate();
	binding->initialize(
		primitive,
		overlayKey,
		texture,
		textureCoordinateId,
		textureCoordinateIndex,
		translation,
		scale
	);

	Ref<CesiumRasterOverlayBinding> replaced =
		tile->add_raster_overlay_binding(binding);
	if (replaced.is_valid()) {
		this->notify_raster_overlay_detaching(replaced);
		replaced->clear_from_material(replaced->get_material());
		replaced->mark_detached();
	}

	tile->refresh_raster_overlay_material(primitive->get_surface_index());
	this->notify_raster_overlay_attached(binding);
	return binding;
}

bool Cesium3DTileset::detach_raster_overlay(
	const Ref<CesiumLoadedTilePrimitive>& primitive,
	const String& overlayKey,
	const Ref<Texture2D>& expectedTexture
) {
	if (primitive.is_null()) {
		return false;
	}
	Cesium3DTile* tile = primitive->get_loaded_tile();
	if (tile == nullptr || tile->get_tileset() != this) {
		return false;
	}

	Ref<CesiumRasterOverlayBinding> removed =
		tile->remove_raster_overlay_binding(
			primitive->get_surface_index(),
			overlayKey,
			expectedTexture
		);
	if (removed.is_null()) {
		return false;
	}

	this->notify_raster_overlay_detaching(removed);
	removed->clear_from_material(removed->get_material());
	tile->refresh_raster_overlay_material(primitive->get_surface_index());
	removed->mark_detached();
	return true;
}

void Cesium3DTileset::recreate_tileset()
{
	// Destroying the Native tileset cancels or drains its outstanding requests
	// while the Godot node and lifecycle receiver are still alive. The next
	// update creates the replacement from the current source settings.
	this->reset_movement_prediction();
	this->cancel_height_requests(
		"Tileset source changed before height sampling completed."
	);
	this->m_tilesetBounds.unref();
	this->release_active_tileset();
	if (this->m_loadFailureQueue != nullptr) {
		this->m_loadFailureQueue->clear();
	}
	this->m_requestCacheDatabase.reset();
	this->m_resolvedHttpCachePath = String();
	this->m_initialLoadingFinished = false;
	this->m_reportedFailedTiles.clear();
	this->m_terminalTileFailureCount = 0;
	this->m_runtimeStatistics =
		std::make_shared<CesiumTilesetRuntimeStatistics>();
	this->m_lastWorkerQueueLength = 0;
	this->m_lastMainQueueLength = 0;
	this->m_lastSelectedTileCount = 0;
	this->m_lastFadingTileCount = 0;
	this->m_lastTilesVisited = 0;
	this->m_lastCulledTilesVisited = 0;
	this->m_lastTilesCulled = 0;
	this->m_lastTilesOccluded = 0;
	this->m_lastTilesWaitingForOcclusionResults = 0;
	this->m_lastTilesKicked = 0;
	this->m_lastMaximumDepthVisited = 0;
	this->m_lastViewStateValid = false;
	this->m_lastViewProjectionType = 0;
	this->m_lastViewProjectionTypeName = "unavailable";
	this->m_lastViewKeepWidth = false;
	this->m_lastViewViewportSize = Vector2();
	this->m_lastViewPosition = glm::dvec3(0.0);
	this->m_lastViewDirection = glm::dvec3(0.0);
	this->m_lastViewUp = glm::dvec3(0.0);
	this->m_lastViewHorizontalFieldOfViewRadians = 0.0;
	this->m_lastViewVerticalFieldOfViewRadians = 0.0;
	this->m_lastViewPlaneExtents = Vector4();
	this->m_lastViewNearPlane = 0.0;
	this->m_lastSelectionViewCount = 0;
	this->m_lastSelectionRenderViewCount = 0;
	this->m_lastSelectionInvalidCameraCount = 0;
	this->m_lastSelectionWrongWorldCameraCount = 0;
	this->m_lastSelectionDuplicateCameraCount = 0;
	this->m_lastSelectionCameraManagerConfigured = false;
	this->m_lastSelectionCameraManagerResolved = false;
	this->m_lastLodTransitionActiveTileCount = 0;
	this->m_lastLodTransitionSupportedPrimitiveCount = 0;
	this->m_lastLodTransitionUnsupportedPrimitiveCount = 0;
	this->m_lastLodTransitionCompatibleRenderNodeCount = 0;
	this->m_lastLodTransitionMinimumPercentage = 1.0;
	this->m_lastLodTransitionMaximumPercentage = 1.0;
	this->m_selectionMicroseconds = 0;
	this->m_loadTilesMicroseconds = 0;
	this->m_debugTileStates.clear();
	this->m_debugTileStatesTruncated = false;
}

void Cesium3DTileset::release_active_tileset() {
	// Provider nodes own their Native overlays. Detach each generation before
	// destroying its collection so a subsequent source/worker recreation can
	// create and attach fresh providers instead of retaining stale instances.
	if (this->m_activeTileset != nullptr) {
		for (int32_t index = 0; index < this->get_child_count(); ++index) {
			CesiumRasterOverlay* overlay = Object::cast_to<CesiumRasterOverlay>(
				this->get_child(index)
			);
			if (overlay != nullptr) {
				overlay->remove_from_tileset(this);
			}
		}
	}
	// Reset Native first so every realized tile and incomplete main-thread
	// result releases its material references. Keep the provider explicitly
	// owned until its finite per-generation shader cache has then been cleared
	// on this Godot main thread. Async CPU results may retain the cache object
	// briefly, but cannot retain its Godot Shader resources across generations.
	const std::shared_ptr<NetworkAssetAccessor> networkAccessor =
		this->m_networkAssetAccessor;
	this->m_activeTileset.reset();
	if (networkAccessor != nullptr) {
		networkAccessor->cancel_all();
	}
	if (this->m_renderResourcesProvider != nullptr) {
		this->m_renderResourcesProvider->release_generation_resources();
		this->m_renderResourcesProvider.reset();
	}
	this->m_networkAssetAccessor.reset();
	this->m_debugTileStates.clear();
	this->m_debugTileStatesTruncated = false;
}

void Cesium3DTileset::reset_movement_prediction() {
	// Destroying the view group unregisters it before the Native tileset or its
	// content manager can be released. It also drops traversal references to
	// prefetched tiles, making them normally eligible for bounded LRU eviction.
	this->m_predictionViewGroup.reset();
	this->m_cameraPredictor.reset();
	this->m_predictionCameraId = ObjectID();
	this->m_lastPredictionActive = false;
	this->m_lastTranslationPredictionActive = false;
	this->m_lastTurnPredictionActive = false;
	this->m_lastZoomOutPredictionActive = false;
	this->m_lastPredictionSpeed = 0.0;
	this->m_lastPredictionDistance = 0.0;
	this->m_lastPredictionAngularSpeedDegrees = 0.0;
	this->m_lastPredictionAngleDegrees = 0.0;
	this->m_lastPredictionZoomOutRate = 0.0;
	this->m_lastPredictionProjectionScale = 1.0;
	this->m_lastPredictedViewDirection = glm::dvec3(0.0, 0.0, -1.0);
	this->m_lastPredictionWorkerQueueLength = 0;
	this->m_lastPredictionMainQueueLength = 0;
	this->m_lastPredictionSuppressedByLodTransitions = false;
}

void Cesium3DTileset::load_tileset()
{	
	//Get the options to read the tileset and then load it into memory
	const Cesium3DTilesSelection::TilesetOptions& options = this->m_tilesetConfig->options;
	if (this->m_selectedDataSource == CesiumDataSource::FromCesiumIon) {
		const String& token = CesiumGDConfig::get_singleton(this)->get_access_token();
		this->m_activeTileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
			this->create_tileset_externals(),
			this->m_cesiumIonAssetId,
			token.utf8().get_data(),
			options
		);
	}
	//Else this is coming from a URL
	else {
		this->m_activeTileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
			this->create_tileset_externals(),
			this->m_url.utf8().get_data(),
			options
		);
	}

	int32_t childCount = this->get_child_count();
	for (int32_t i = 0; i < childCount; i++)
	{
		Node* currChild = this->get_child(i);
		CesiumRasterOverlay* overlay = Object::cast_to<CesiumRasterOverlay>(currChild);
		if (overlay == nullptr) continue;
		overlay->add_to_tileset(this);
	}

}

Cesium3DTilesSelection::TilesetExternals Cesium3DTileset::create_tileset_externals()
{
	CesiumNetworkRetryOptions retryOptions;
	retryOptions.maximumRetries = static_cast<uint32_t>(
		this->m_maximumNetworkRetries
	);
	retryOptions.initialDelaySeconds = static_cast<double>(
		this->m_networkRetryInitialDelaySeconds
	);
	retryOptions.maximumDelaySeconds = static_cast<double>(
		this->m_networkRetryMaximumDelaySeconds
	);
	auto simpleAccessor = std::make_shared<NetworkAssetAccessor>(
		this->m_runtimeStatistics,
		this->m_loadFailureQueue,
		static_cast<uint64_t>(this->get_instance_id()),
		retryOptions
	);
	this->m_networkAssetAccessor = simpleAccessor;
	std::shared_ptr<CesiumAsync::IAssetAccessor> requestAccessor =
		simpleAccessor;
	this->m_requestCacheDatabase.reset();
	this->m_resolvedHttpCachePath = resolve_cache_path(this->m_httpCachePath);
	if (this->m_httpCacheEnabled) {
		if (this->m_resolvedHttpCachePath.is_empty()) {
			ERR_PRINT("HTTP cache is enabled but http_cache_path is empty");
			CesiumLoadFailureRecord record;
			record.category = CesiumLoadFailure::Category::Cache;
			record.stage = CesiumLoadFailure::Stage::CacheOpen;
			record.terminal = false;
			record.message =
				"HTTP cache is enabled but http_cache_path is empty";
			this->m_loadFailureQueue->push(std::move(record));
		} else {
			const String cacheDirectory =
				this->m_resolvedHttpCachePath.get_base_dir();
			const Error directoryError =
				DirAccess::make_dir_recursive_absolute(cacheDirectory);
			if (directoryError != Error::OK) {
				ERR_PRINT(
					"Could not create HTTP cache directory: " + cacheDirectory
				);
				CesiumLoadFailureRecord record;
				record.category = CesiumLoadFailure::Category::Cache;
				record.stage = CesiumLoadFailure::Stage::CacheOpen;
				record.terminal = false;
				record.message = "Could not create HTTP cache directory";
				record.url = this->m_resolvedHttpCachePath.utf8().get_data();
				this->m_loadFailureQueue->push(std::move(record));
			} else {
				try {
					auto sqliteCache =
						std::make_shared<CesiumAsync::SqliteCache>(
							spdlog::default_logger(),
							this->m_resolvedHttpCachePath.utf8().get_data(),
							this->m_httpCacheMaximumItems,
							this->m_httpCacheMaximumDataBytes
						);
					this->m_requestCacheDatabase =
						std::make_shared<InstrumentedCacheDatabase>(
							sqliteCache,
							this->m_runtimeStatistics,
							this->m_loadFailureQueue,
							static_cast<uint64_t>(this->get_instance_id())
						);
					requestAccessor =
						std::make_shared<CesiumAsync::CachingAssetAccessor>(
							spdlog::default_logger(),
							simpleAccessor,
							this->m_requestCacheDatabase,
							this->m_httpCachePruneIntervalRequests
						);
				} catch (const std::exception& exception) {
					ERR_PRINT(
						"Could not open HTTP cache " +
						this->m_resolvedHttpCachePath + ": " +
						exception.what()
					);
					CesiumLoadFailureRecord record;
					record.category = CesiumLoadFailure::Category::Cache;
					record.stage = CesiumLoadFailure::Stage::CacheOpen;
					record.terminal = false;
					record.message = exception.what();
					record.url =
						this->m_resolvedHttpCachePath.utf8().get_data();
					this->m_loadFailureQueue->push(std::move(record));
				}
			}
		}
	}
	auto gunzipAccessor = std::make_shared<CesiumAsync::GunzipAssetAccessor>(
		requestAccessor
	);
	
	auto taskProcessor = std::make_shared<GodotTaskProcessor>(
		this->m_workerThreadCount
	);
	CesiumAsync::AsyncSystem asyncSystem(taskProcessor);
	this->m_renderResourcesProvider =
		std::make_shared<GodotPrepareRenderResources>(
			this,
			this->m_runtimeStatistics,
			this->m_loadFailureQueue
		);
	CesiumGDCreditSystem* godotCreditSystem = this->resolve_credit_system();
	std::shared_ptr<CesiumUtility::CreditSystem> creditSystem =
		godotCreditSystem == nullptr
			? std::shared_ptr<CesiumUtility::CreditSystem>()
			: godotCreditSystem->get_native_credit_system();
	
	Cesium3DTilesSelection::TilesetExternals result {
		gunzipAccessor,
		this->m_renderResourcesProvider,
		asyncSystem,
		creditSystem
	};
	return result;
}

void Cesium3DTileset::render_tile_as_node(const Cesium3DTilesSelection::Tile& tile)
{
	if (tile.getState() == Cesium3DTilesSelection::TileLoadState::Failed) {
		// Raster-overlay refinement creates synthetic quadtree children by
		// clipping the parent mesh. A child that covers no parent triangles is
		// expected to produce no model and is not a source-content failure.
		if (std::holds_alternative<CesiumGeometry::UpsampledQuadtreeNode>(
				tile.getTileID())) {
			return;
		}
		std::string tileIdStr = Cesium3DTilesSelection::TileIdUtilities::createTileIdString(tile.getTileID());
		if (this->m_reportedFailedTiles.insert(tileIdStr).second) {
			ERR_PRINT(String("Failed to load tile ") + tileIdStr.c_str());
		}
		return;
	}
	
	if (tile.getState() != Cesium3DTilesSelection::TileLoadState::Done) {
		return;
	}

	const Cesium3DTilesSelection::TileContent& content = tile.getContent();
	const Cesium3DTilesSelection::TileRenderContent* renderContent = content.getRenderContent();
	
	if (renderContent == nullptr) {
		return;
	}
	Cesium3DTile* foundNode = static_cast<Cesium3DTile*>(renderContent->getRenderResources());
	if (foundNode == nullptr) return;
	
	if (this->m_createPhysicsMeshes) {
		foundNode->set_tile_collision_enabled(true);
	}

	if (!foundNode->is_inside_tree()) {
		size_t hash = std::visit(CesiumVariantHash{}, tile.getTileID());
		this->register_tile(foundNode, hash);
		foundNode->set_name(itos(hash));
	}
	
	if (!this->m_debugVolumesFunction.is_null()) {
		// Get the xform for the current tile rotation
		// Basis + Zero pos * ecef_engine_xform()
		draw_debug_volume_from_variant(tile.getBoundingVolume(), this->m_debugVolumesFunction, this->m_georeference);
	}

	if (!this->m_forEachTileFunction.is_null()) {
		godot::Array callback_args;
		callback_args.push_back(foundNode);
		this->m_forEachTileFunction.callv(callback_args);
	}

	const bool wasVisible = foundNode->is_visible();
	foundNode->show();
	if (!wasVisible) {
		this->notify_tile_visibility_changed(foundNode, true);
	}
}

void Cesium3DTileset::despawn_tile(const Cesium3DTilesSelection::Tile& tile)
{
	if (tile.getState() != Cesium3DTilesSelection::TileLoadState::Done) {
		return;
	}
	const Cesium3DTilesSelection::TileRenderContent* renderContent = tile.getContent().getRenderContent();
	if (renderContent == nullptr) return;
	Cesium3DTile* foundNode = static_cast<Cesium3DTile*>(renderContent->getRenderResources());
	if (foundNode == nullptr) return;
	if (!foundNode->is_inside_tree()) return;
	const bool wasVisible = foundNode->is_visible();
	foundNode->hide();
	if (wasVisible) {
		this->notify_tile_visibility_changed(foundNode, false);
	}
	// Deactivate the collisions
	if (this->m_createPhysicsMeshes) {
		foundNode->set_tile_collision_enabled(false);
	}
}

void Cesium3DTileset::despawn_tile_deferred(const Cesium3DTilesSelection::Tile& tile)
{
}

bool Cesium3DTileset::try_get_tile_from_instance_id(const ObjectID& objectId, Cesium3DTile** outNode)
{
	*outNode = Object::cast_to<Cesium3DTile>(ObjectDB::get_instance(objectId));
	return *outNode != nullptr;
}


void Cesium3DTileset::register_tile(Cesium3DTile *instance, size_t hash) {
	this->add_child(instance, false);
	instance->set_owner(this);
	instance->apply_georeference();
}


bool Cesium3DTileset::get_show_hierarchy() const {
	return this->m_showHierarchy;
}

void Cesium3DTileset::set_show_hierarchy(bool show) {
	this->m_showHierarchy = show;
}

void Cesium3DTileset::_bind_methods()
{
#pragma region Inspector properties
	ClassDB::bind_method(D_METHOD("set_maximum_screen_space_error", "error"), &Cesium3DTileset::set_maximum_screen_space_error);
	ClassDB::bind_method(D_METHOD("get_maximum_screen_space_error"), &Cesium3DTileset::get_maximum_screen_space_error);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "maximum_screen_space_error", PROPERTY_HINT_NONE, MAXIMUM_SCREEN_SPACE_DESC), "set_maximum_screen_space_error", "get_maximum_screen_space_error");

	
	ClassDB::bind_method(D_METHOD("set_maximum_simultaneous_tile_loads", "count"), &Cesium3DTileset::set_maximum_simultaneous_tile_loads);
	ClassDB::bind_method(D_METHOD("get_maximum_simultaneous_tile_loads"), &Cesium3DTileset::get_maximum_simultaneous_tile_loads);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "maximum_simultaneous_tile_loads", PROPERTY_HINT_NONE, MAXIMUM_SIMULTANEOUS_TILE_LOADS_DESC), "set_maximum_simultaneous_tile_loads", "get_maximum_simultaneous_tile_loads");

	ClassDB::bind_method(
		D_METHOD("set_stale_request_cancellation_enabled", "enabled"),
		&Cesium3DTileset::set_stale_request_cancellation_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_stale_request_cancellation_enabled"),
		&Cesium3DTileset::get_stale_request_cancellation_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"stale_request_cancellation_enabled",
			PROPERTY_HINT_NONE,
			STALE_REQUEST_CANCELLATION_DESC
		),
		"set_stale_request_cancellation_enabled",
		"get_stale_request_cancellation_enabled"
	);

	ClassDB::bind_method(D_METHOD("set_preload_ancestors", "preload"), &Cesium3DTileset::set_preload_ancestors);
	ClassDB::bind_method(D_METHOD("get_preload_ancestors"), &Cesium3DTileset::get_preload_ancestors);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preload_ancestors", PROPERTY_HINT_NONE, PRELOAD_ANCESTORS_DESC), "set_preload_ancestors", "get_preload_ancestors");

	ClassDB::bind_method(D_METHOD("set_preload_siblings", "preload"), &Cesium3DTileset::set_preload_siblings);
	ClassDB::bind_method(D_METHOD("get_preload_siblings"), &Cesium3DTileset::get_preload_siblings);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "preload_siblings", PROPERTY_HINT_NONE, PRELOAD_SIBLINGS_DESC), "set_preload_siblings", "get_preload_siblings");

	ClassDB::bind_method(D_METHOD("set_loading_descendant_limit", "limit"), &Cesium3DTileset::set_loading_descendant_limit);
	ClassDB::bind_method(D_METHOD("get_loading_descendant_limit"), &Cesium3DTileset::get_loading_descendant_limit);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loading_descendant_limit", PROPERTY_HINT_NONE, LOADING_DESCENDANT_LIMIT_DESC), "set_loading_descendant_limit", "get_loading_descendant_limit");

	ClassDB::bind_method(D_METHOD("set_forbid_holes", "forbidHoles"), &Cesium3DTileset::set_forbid_holes);
	ClassDB::bind_method(D_METHOD("get_forbid_holes"), &Cesium3DTileset::get_forbid_holes);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "forbid_holes", PROPERTY_HINT_NONE, FORBID_HOLES_DESC), "set_forbid_holes", "get_forbid_holes");

	ClassDB::bind_method(
		D_METHOD("set_frustum_culling_enabled", "enabled"),
		&Cesium3DTileset::set_frustum_culling_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_frustum_culling_enabled"),
		&Cesium3DTileset::get_frustum_culling_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"frustum_culling_enabled",
			PROPERTY_HINT_NONE,
			FRUSTUM_CULLING_DESC
		),
		"set_frustum_culling_enabled",
		"get_frustum_culling_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_fog_culling_enabled", "enabled"),
		&Cesium3DTileset::set_fog_culling_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_fog_culling_enabled"),
		&Cesium3DTileset::get_fog_culling_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"fog_culling_enabled",
			PROPERTY_HINT_NONE,
			FOG_CULLING_DESC
		),
		"set_fog_culling_enabled",
		"get_fog_culling_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_enforce_culled_screen_space_error", "enforce"),
		&Cesium3DTileset::set_enforce_culled_screen_space_error
	);
	ClassDB::bind_method(
		D_METHOD("get_enforce_culled_screen_space_error"),
		&Cesium3DTileset::get_enforce_culled_screen_space_error
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"enforce_culled_screen_space_error",
			PROPERTY_HINT_NONE,
			ENFORCE_CULLED_SSE_DESC
		),
		"set_enforce_culled_screen_space_error",
		"get_enforce_culled_screen_space_error"
	);

	ClassDB::bind_method(
		D_METHOD("set_culled_screen_space_error", "error"),
		&Cesium3DTileset::set_culled_screen_space_error
	);
	ClassDB::bind_method(
		D_METHOD("get_culled_screen_space_error"),
		&Cesium3DTileset::get_culled_screen_space_error
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"culled_screen_space_error",
			PROPERTY_HINT_RANGE,
			"0,4096,0.1,or_greater"
		),
		"set_culled_screen_space_error",
		"get_culled_screen_space_error"
	);

	ClassDB::bind_method(
		D_METHOD("set_render_tiles_under_camera", "enabled"),
		&Cesium3DTileset::set_render_tiles_under_camera
	);
	ClassDB::bind_method(
		D_METHOD("get_render_tiles_under_camera"),
		&Cesium3DTileset::get_render_tiles_under_camera
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"render_tiles_under_camera",
			PROPERTY_HINT_NONE,
			RENDER_TILES_UNDER_CAMERA_DESC
		),
		"set_render_tiles_under_camera",
		"get_render_tiles_under_camera"
	);

	ClassDB::bind_method(
		D_METHOD("set_lod_transitions_enabled", "enabled"),
		&Cesium3DTileset::set_lod_transitions_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_lod_transitions_enabled"),
		&Cesium3DTileset::get_lod_transitions_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"lod_transitions_enabled",
			PROPERTY_HINT_NONE,
			LOD_TRANSITIONS_DESC
		),
		"set_lod_transitions_enabled",
		"get_lod_transitions_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_lod_transition_length", "seconds"),
		&Cesium3DTileset::set_lod_transition_length
	);
	ClassDB::bind_method(
		D_METHOD("get_lod_transition_length"),
		&Cesium3DTileset::get_lod_transition_length
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"lod_transition_length",
			PROPERTY_HINT_RANGE,
			"0.001,10,0.01,or_greater,suffix:s"
		),
		"set_lod_transition_length",
		"get_lod_transition_length"
	);

	ClassDB::bind_method(
		D_METHOD("set_kick_descendants_while_fading_in", "enabled"),
		&Cesium3DTileset::set_kick_descendants_while_fading_in
	);
	ClassDB::bind_method(
		D_METHOD("get_kick_descendants_while_fading_in"),
		&Cesium3DTileset::get_kick_descendants_while_fading_in
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"kick_descendants_while_fading_in",
			PROPERTY_HINT_NONE,
			KICK_DESCENDANTS_WHILE_FADING_DESC
		),
		"set_kick_descendants_while_fading_in",
		"get_kick_descendants_while_fading_in"
	);

	ClassDB::bind_method(
		D_METHOD("set_translucency_sort_priority", "priority"),
		&Cesium3DTileset::set_translucency_sort_priority
	);
	ClassDB::bind_method(
		D_METHOD("get_translucency_sort_priority"),
		&Cesium3DTileset::get_translucency_sort_priority
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"translucency_sort_priority",
			PROPERTY_HINT_RANGE,
			"-128,127,1"
		),
		"set_translucency_sort_priority",
		"get_translucency_sort_priority"
	);

	ClassDB::bind_method(
		D_METHOD("set_translucency_depth_prepass_enabled", "enabled"),
		&Cesium3DTileset::set_translucency_depth_prepass_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_translucency_depth_prepass_enabled"),
		&Cesium3DTileset::get_translucency_depth_prepass_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"translucency_depth_prepass_enabled",
			PROPERTY_HINT_NONE,
			TRANSLUCENCY_DEPTH_PREPASS_DESC
		),
		"set_translucency_depth_prepass_enabled",
		"get_translucency_depth_prepass_enabled"
	);

	ClassDB::bind_method(D_METHOD("set_maximum_cached_bytes", "bytes"), &Cesium3DTileset::set_maximum_cached_bytes);
	ClassDB::bind_method(D_METHOD("get_maximum_cached_bytes"), &Cesium3DTileset::get_maximum_cached_bytes);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"maximum_cached_bytes",
			PROPERTY_HINT_RANGE,
			"0,17179869184,1048576,or_greater,suffix:B"
		),
		"set_maximum_cached_bytes",
		"get_maximum_cached_bytes"
	);
	ClassDB::bind_method(
		D_METHOD("set_automatic_hardware_budgets_enabled", "enabled"),
		&Cesium3DTileset::set_automatic_hardware_budgets_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_automatic_hardware_budgets_enabled"),
		&Cesium3DTileset::get_automatic_hardware_budgets_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"automatic_hardware_budgets_enabled",
			PROPERTY_HINT_NONE,
			AUTOMATIC_HARDWARE_BUDGETS_DESC
		),
		"set_automatic_hardware_budgets_enabled",
		"get_automatic_hardware_budgets_enabled"
	);
	ClassDB::bind_method(
		D_METHOD("set_hardware_budget_profile", "profile"),
		&Cesium3DTileset::set_hardware_budget_profile
	);
	ClassDB::bind_method(
		D_METHOD("get_hardware_budget_profile"),
		&Cesium3DTileset::get_hardware_budget_profile
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"hardware_budget_profile",
			PROPERTY_HINT_ENUM,
			"Conservative,Balanced,Aggressive"
		),
		"set_hardware_budget_profile",
		"get_hardware_budget_profile"
	);
	ClassDB::bind_method(
		D_METHOD("set_automatic_cache_budget_share", "share"),
		&Cesium3DTileset::set_automatic_cache_budget_share
	);
	ClassDB::bind_method(
		D_METHOD("get_automatic_cache_budget_share"),
		&Cesium3DTileset::get_automatic_cache_budget_share
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"automatic_cache_budget_share",
			PROPERTY_HINT_RANGE,
			"0,1,0.01",
			PROPERTY_USAGE_DEFAULT
		),
		"set_automatic_cache_budget_share",
		"get_automatic_cache_budget_share"
	);
	ClassDB::bind_method(
		D_METHOD("recalculate_automatic_hardware_budgets"),
		&Cesium3DTileset::recalculate_automatic_hardware_budgets
	);
	ClassDB::bind_method(
		D_METHOD("get_hardware_capabilities"),
		&Cesium3DTileset::get_hardware_capabilities
	);
	ClassDB::bind_method(
		D_METHOD("get_recommended_total_cache_bytes", "profile"),
		&Cesium3DTileset::get_recommended_total_cache_bytes
	);
	ClassDB::bind_integer_constant(
		get_class_static(),
		"CesiumHardwareBudgetProfile",
		"HardwareBudgetConservative",
		static_cast<int32_t>(CesiumHardwareBudgetProfile::Conservative)
	);
	ClassDB::bind_integer_constant(
		get_class_static(),
		"CesiumHardwareBudgetProfile",
		"HardwareBudgetBalanced",
		static_cast<int32_t>(CesiumHardwareBudgetProfile::Balanced)
	);
	ClassDB::bind_integer_constant(
		get_class_static(),
		"CesiumHardwareBudgetProfile",
		"HardwareBudgetAggressive",
		static_cast<int32_t>(CesiumHardwareBudgetProfile::Aggressive)
	);

	ClassDB::bind_method(D_METHOD("set_worker_thread_count", "count"), &Cesium3DTileset::set_worker_thread_count);
	ClassDB::bind_method(D_METHOD("get_worker_thread_count"), &Cesium3DTileset::get_worker_thread_count);
	ClassDB::bind_method(
		D_METHOD("reset_worker_thread_count_to_automatic"),
		&Cesium3DTileset::reset_worker_thread_count_to_automatic
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"worker_thread_count",
			PROPERTY_HINT_RANGE,
			"1,64,1"
		),
		"set_worker_thread_count",
		"get_worker_thread_count"
	);

	ClassDB::bind_method(
		D_METHOD("set_main_thread_loading_time_limit_ms", "milliseconds"),
		&Cesium3DTileset::set_main_thread_loading_time_limit_ms
	);
	ClassDB::bind_method(
		D_METHOD("get_main_thread_loading_time_limit_ms"),
		&Cesium3DTileset::get_main_thread_loading_time_limit_ms
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"main_thread_loading_time_limit_ms",
			PROPERTY_HINT_RANGE,
			"0,50,0.1,or_greater,suffix:ms"
		),
		"set_main_thread_loading_time_limit_ms",
		"get_main_thread_loading_time_limit_ms"
	);

	ClassDB::bind_method(
		D_METHOD("set_maximum_primitive_geometry_upload_bytes", "bytes"),
		&Cesium3DTileset::set_maximum_primitive_geometry_upload_bytes
	);
	ClassDB::bind_method(
		D_METHOD("get_maximum_primitive_geometry_upload_bytes"),
		&Cesium3DTileset::get_maximum_primitive_geometry_upload_bytes
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"maximum_primitive_geometry_upload_bytes",
			PROPERTY_HINT_RANGE,
			"0,268435456,1048576,or_greater,suffix:B"
		),
		"set_maximum_primitive_geometry_upload_bytes",
		"get_maximum_primitive_geometry_upload_bytes"
	);

	ClassDB::bind_method(
		D_METHOD("set_maximum_primitive_texture_upload_bytes", "bytes"),
		&Cesium3DTileset::set_maximum_primitive_texture_upload_bytes
	);
	ClassDB::bind_method(
		D_METHOD("get_maximum_primitive_texture_upload_bytes"),
		&Cesium3DTileset::get_maximum_primitive_texture_upload_bytes
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"maximum_primitive_texture_upload_bytes",
			PROPERTY_HINT_RANGE,
			"0,268435456,1048576,or_greater,suffix:B"
		),
		"set_maximum_primitive_texture_upload_bytes",
		"get_maximum_primitive_texture_upload_bytes"
	);

	ClassDB::bind_method(
		D_METHOD("set_tile_cache_unload_time_limit_ms", "milliseconds"),
		&Cesium3DTileset::set_tile_cache_unload_time_limit_ms
	);
	ClassDB::bind_method(
		D_METHOD("get_tile_cache_unload_time_limit_ms"),
		&Cesium3DTileset::get_tile_cache_unload_time_limit_ms
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"tile_cache_unload_time_limit_ms",
			PROPERTY_HINT_RANGE,
			"0,50,0.1,or_greater,suffix:ms"
		),
		"set_tile_cache_unload_time_limit_ms",
		"get_tile_cache_unload_time_limit_ms"
	);

	ClassDB::bind_method(
		D_METHOD("set_http_cache_enabled", "enabled"),
		&Cesium3DTileset::set_http_cache_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_http_cache_enabled"),
		&Cesium3DTileset::get_http_cache_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "http_cache_enabled"),
		"set_http_cache_enabled",
		"get_http_cache_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_http_cache_path", "path"),
		&Cesium3DTileset::set_http_cache_path
	);
	ClassDB::bind_method(
		D_METHOD("get_http_cache_path"),
		&Cesium3DTileset::get_http_cache_path
	);
	ClassDB::bind_method(
		D_METHOD("get_resolved_http_cache_path"),
		&Cesium3DTileset::get_resolved_http_cache_path
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::STRING,
			"http_cache_path",
			PROPERTY_HINT_FILE,
			"*.sqlite"
		),
		"set_http_cache_path",
		"get_http_cache_path"
	);

	ClassDB::bind_method(
		D_METHOD("set_http_cache_maximum_items", "maximum_items"),
		&Cesium3DTileset::set_http_cache_maximum_items
	);
	ClassDB::bind_method(
		D_METHOD("get_http_cache_maximum_items"),
		&Cesium3DTileset::get_http_cache_maximum_items
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"http_cache_maximum_items",
			PROPERTY_HINT_RANGE,
			"0,1000000,1,or_greater"
		),
		"set_http_cache_maximum_items",
		"get_http_cache_maximum_items"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_http_cache_maximum_data_bytes",
			"maximum_bytes"
		),
		&Cesium3DTileset::set_http_cache_maximum_data_bytes
	);
	ClassDB::bind_method(
		D_METHOD("get_http_cache_maximum_data_bytes"),
		&Cesium3DTileset::get_http_cache_maximum_data_bytes
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"http_cache_maximum_data_bytes",
			PROPERTY_HINT_RANGE,
			"0,17179869184,1048576,or_greater,suffix:B"
		),
		"set_http_cache_maximum_data_bytes",
		"get_http_cache_maximum_data_bytes"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_http_cache_prune_interval_requests",
			"requests"
		),
		&Cesium3DTileset::set_http_cache_prune_interval_requests
	);
	ClassDB::bind_method(
		D_METHOD("get_http_cache_prune_interval_requests"),
		&Cesium3DTileset::get_http_cache_prune_interval_requests
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"http_cache_prune_interval_requests",
			PROPERTY_HINT_RANGE,
			"1,100000,1,or_greater"
		),
		"set_http_cache_prune_interval_requests",
		"get_http_cache_prune_interval_requests"
	);

	ClassDB::bind_method(
		D_METHOD("clear_http_cache"),
		&Cesium3DTileset::clear_http_cache
	);
	ClassDB::bind_method(
		D_METHOD("prune_http_cache"),
		&Cesium3DTileset::prune_http_cache
	);
	ClassDB::bind_method(
		D_METHOD("set_maximum_network_retries", "retries"),
		&Cesium3DTileset::set_maximum_network_retries
	);
	ClassDB::bind_method(
		D_METHOD("get_maximum_network_retries"),
		&Cesium3DTileset::get_maximum_network_retries
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"maximum_network_retries",
			PROPERTY_HINT_RANGE,
			"0,16,1"
		),
		"set_maximum_network_retries",
		"get_maximum_network_retries"
	);
	ClassDB::bind_method(
		D_METHOD("set_network_retry_initial_delay_seconds", "seconds"),
		&Cesium3DTileset::set_network_retry_initial_delay_seconds
	);
	ClassDB::bind_method(
		D_METHOD("get_network_retry_initial_delay_seconds"),
		&Cesium3DTileset::get_network_retry_initial_delay_seconds
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"network_retry_initial_delay_seconds",
			PROPERTY_HINT_RANGE,
			"0,60,0.05,suffix:s"
		),
		"set_network_retry_initial_delay_seconds",
		"get_network_retry_initial_delay_seconds"
	);
	ClassDB::bind_method(
		D_METHOD("set_network_retry_maximum_delay_seconds", "seconds"),
		&Cesium3DTileset::set_network_retry_maximum_delay_seconds
	);
	ClassDB::bind_method(
		D_METHOD("get_network_retry_maximum_delay_seconds"),
		&Cesium3DTileset::get_network_retry_maximum_delay_seconds
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"network_retry_maximum_delay_seconds",
			PROPERTY_HINT_RANGE,
			"0,300,0.05,suffix:s"
		),
		"set_network_retry_maximum_delay_seconds",
		"get_network_retry_maximum_delay_seconds"
	);

	ClassDB::bind_method(
		D_METHOD("set_movement_prediction_enabled", "enabled"),
		&Cesium3DTileset::set_movement_prediction_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_movement_prediction_enabled"),
		&Cesium3DTileset::get_movement_prediction_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"movement_prediction_enabled",
			PROPERTY_HINT_NONE,
			MOVEMENT_PREDICTION_ENABLED_DESC
		),
		"set_movement_prediction_enabled",
		"get_movement_prediction_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_movement_prediction_seconds", "seconds"),
		&Cesium3DTileset::set_movement_prediction_seconds
	);
	ClassDB::bind_method(
		D_METHOD("get_movement_prediction_seconds"),
		&Cesium3DTileset::get_movement_prediction_seconds
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"movement_prediction_seconds",
			PROPERTY_HINT_RANGE,
			"0,10,0.05,suffix:s"
		),
		"set_movement_prediction_seconds",
		"get_movement_prediction_seconds"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_movement_prediction_minimum_speed",
			"meters_per_second"
		),
		&Cesium3DTileset::set_movement_prediction_minimum_speed
	);
	ClassDB::bind_method(
		D_METHOD("get_movement_prediction_minimum_speed"),
		&Cesium3DTileset::get_movement_prediction_minimum_speed
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"movement_prediction_minimum_speed",
			PROPERTY_HINT_RANGE,
			"0,10000,0.1,or_greater,suffix:m/s"
		),
		"set_movement_prediction_minimum_speed",
		"get_movement_prediction_minimum_speed"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_movement_prediction_maximum_distance",
			"meters"
		),
		&Cesium3DTileset::set_movement_prediction_maximum_distance
	);
	ClassDB::bind_method(
		D_METHOD("get_movement_prediction_maximum_distance"),
		&Cesium3DTileset::get_movement_prediction_maximum_distance
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"movement_prediction_maximum_distance",
			PROPERTY_HINT_RANGE,
			"0,100000,1,or_greater,suffix:m"
		),
		"set_movement_prediction_maximum_distance",
		"get_movement_prediction_maximum_distance"
	);

	ClassDB::bind_method(
		D_METHOD("set_movement_prediction_weight", "weight"),
		&Cesium3DTileset::set_movement_prediction_weight
	);
	ClassDB::bind_method(
		D_METHOD("get_movement_prediction_weight"),
		&Cesium3DTileset::get_movement_prediction_weight
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"movement_prediction_weight",
			PROPERTY_HINT_RANGE,
			"0.01,10,0.01,or_greater"
		),
		"set_movement_prediction_weight",
		"get_movement_prediction_weight"
	);

	ClassDB::bind_method(
		D_METHOD("set_turn_prediction_enabled", "enabled"),
		&Cesium3DTileset::set_turn_prediction_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_turn_prediction_enabled"),
		&Cesium3DTileset::get_turn_prediction_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"turn_prediction_enabled",
			PROPERTY_HINT_NONE,
			TURN_PREDICTION_ENABLED_DESC
		),
		"set_turn_prediction_enabled",
		"get_turn_prediction_enabled"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_turn_prediction_minimum_angular_speed_degrees",
			"degrees_per_second"
		),
		&Cesium3DTileset::set_turn_prediction_minimum_angular_speed_degrees
	);
	ClassDB::bind_method(
		D_METHOD("get_turn_prediction_minimum_angular_speed_degrees"),
		&Cesium3DTileset::get_turn_prediction_minimum_angular_speed_degrees
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"turn_prediction_minimum_angular_speed_degrees",
			PROPERTY_HINT_RANGE,
			"0,720,0.1,or_greater,suffix:deg/s"
		),
		"set_turn_prediction_minimum_angular_speed_degrees",
		"get_turn_prediction_minimum_angular_speed_degrees"
	);

	ClassDB::bind_method(
		D_METHOD(
			"set_turn_prediction_maximum_angle_degrees",
			"degrees"
		),
		&Cesium3DTileset::set_turn_prediction_maximum_angle_degrees
	);
	ClassDB::bind_method(
		D_METHOD("get_turn_prediction_maximum_angle_degrees"),
		&Cesium3DTileset::get_turn_prediction_maximum_angle_degrees
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"turn_prediction_maximum_angle_degrees",
			PROPERTY_HINT_RANGE,
			"0,180,0.1,suffix:deg"
		),
		"set_turn_prediction_maximum_angle_degrees",
		"get_turn_prediction_maximum_angle_degrees"
	);

	ClassDB::bind_method(
		D_METHOD("set_zoom_out_prediction_enabled", "enabled"),
		&Cesium3DTileset::set_zoom_out_prediction_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_zoom_out_prediction_enabled"),
		&Cesium3DTileset::get_zoom_out_prediction_enabled
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"zoom_out_prediction_enabled",
			PROPERTY_HINT_NONE,
			ZOOM_OUT_PREDICTION_ENABLED_DESC
		),
		"set_zoom_out_prediction_enabled",
		"get_zoom_out_prediction_enabled"
	);

	ClassDB::bind_method(
		D_METHOD("set_zoom_out_prediction_minimum_rate", "rate"),
		&Cesium3DTileset::set_zoom_out_prediction_minimum_rate
	);
	ClassDB::bind_method(
		D_METHOD("get_zoom_out_prediction_minimum_rate"),
		&Cesium3DTileset::get_zoom_out_prediction_minimum_rate
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"zoom_out_prediction_minimum_rate",
			PROPERTY_HINT_RANGE,
			"0,10,0.01,or_greater,suffix:1/s"
		),
		"set_zoom_out_prediction_minimum_rate",
		"get_zoom_out_prediction_minimum_rate"
	);

	ClassDB::bind_method(
		D_METHOD("set_zoom_out_prediction_maximum_scale", "scale"),
		&Cesium3DTileset::set_zoom_out_prediction_maximum_scale
	);
	ClassDB::bind_method(
		D_METHOD("get_zoom_out_prediction_maximum_scale"),
		&Cesium3DTileset::get_zoom_out_prediction_maximum_scale
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::FLOAT,
			"zoom_out_prediction_maximum_scale",
			PROPERTY_HINT_RANGE,
			"1,16,0.05,or_greater"
		),
		"set_zoom_out_prediction_maximum_scale",
		"get_zoom_out_prediction_maximum_scale"
	);

	ClassDB::bind_method(D_METHOD("set_generate_missing_normals_smooth", "shouldGenerate"), &Cesium3DTileset::set_generate_missing_normals_smooth);
	ClassDB::bind_method(D_METHOD("get_generate_missing_normals_smooth"), &Cesium3DTileset::get_generate_missing_normals_smooth);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_missing_normals_smooth", PROPERTY_HINT_NONE, GENERATE_MISSING_NORMALS_DESC), "set_generate_missing_normals_smooth", "get_generate_missing_normals_smooth");

	ClassDB::bind_method(D_METHOD("set_create_physics_meshes", "shouldGenerate"), &Cesium3DTileset::set_create_physics_meshes);
	ClassDB::bind_method(D_METHOD("get_create_physics_meshes"), &Cesium3DTileset::get_create_physics_meshes);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "create_physics_meshes"), "set_create_physics_meshes", "get_create_physics_meshes");

	ClassDB::bind_method(
		D_METHOD("set_camera_manager_path", "camera_manager_path"),
		&Cesium3DTileset::set_camera_manager_path
	);
	ClassDB::bind_method(
		D_METHOD("get_camera_manager_path"),
		&Cesium3DTileset::get_camera_manager_path
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::NODE_PATH,
			"camera_manager_path",
			PROPERTY_HINT_NODE_PATH_VALID_TYPES,
			"CesiumCameraManager"
		),
		"set_camera_manager_path",
		"get_camera_manager_path"
	);

	ClassDB::bind_method(
		D_METHOD("set_credit", "credit"),
		&Cesium3DTileset::set_credit
	);
	ClassDB::bind_method(
		D_METHOD("get_credit"),
		&Cesium3DTileset::get_credit
	);
	ClassDB::bind_method(
		D_METHOD("set_credit_system", "credit_system"),
		&Cesium3DTileset::set_credit_system
	);
	ClassDB::bind_method(
		D_METHOD("get_credit_system"),
		&Cesium3DTileset::get_credit_system
	);
	ClassDB::bind_method(
		D_METHOD("resolve_credit_system"),
		&Cesium3DTileset::resolve_credit_system
	);
	ClassDB::bind_method(
		D_METHOD("get_resolved_credit_system"),
		&Cesium3DTileset::get_resolved_credit_system
	);
	ClassDB::bind_method(
		D_METHOD("invalidate_resolved_credit_system"),
		&Cesium3DTileset::invalidate_resolved_credit_system
	);
	ClassDB::bind_method(
		D_METHOD("set_show_credits_on_screen", "show_on_screen"),
		&Cesium3DTileset::set_show_credits_on_screen
	);
	ClassDB::bind_method(
		D_METHOD("get_show_credits_on_screen"),
		&Cesium3DTileset::get_show_credits_on_screen
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::STRING,
			"credit",
			PROPERTY_HINT_MULTILINE_TEXT
		),
		"set_credit",
		"get_credit"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"credit_system",
			PROPERTY_HINT_NODE_TYPE,
			"CesiumGDCreditSystem"
		),
		"set_credit_system",
		"get_credit_system"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"resolved_credit_system",
			PROPERTY_HINT_NODE_TYPE,
			"CesiumGDCreditSystem",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_resolved_credit_system"
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "show_credits_on_screen"),
		"set_show_credits_on_screen",
		"get_show_credits_on_screen"
	);

	ClassDB::bind_method(D_METHOD("set_lifecycle_event_receiver", "receiver"), &Cesium3DTileset::set_lifecycle_event_receiver);
	ClassDB::bind_method(D_METHOD("get_lifecycle_event_receiver"), &Cesium3DTileset::get_lifecycle_event_receiver);
	ClassDB::bind_method(D_METHOD("set_metadata_style", "style"), &Cesium3DTileset::set_metadata_style);
	ClassDB::bind_method(D_METHOD("get_metadata_style"), &Cesium3DTileset::get_metadata_style);
	ClassDB::bind_method(D_METHOD("refresh_metadata_style"), &Cesium3DTileset::refresh_metadata_style);
	ClassDB::bind_method(D_METHOD("_on_metadata_style_changed"), &Cesium3DTileset::_on_metadata_style_changed);
	ClassDB::bind_method(D_METHOD("set_point_cloud_shading", "shading"), &Cesium3DTileset::set_point_cloud_shading);
	ClassDB::bind_method(D_METHOD("get_point_cloud_shading"), &Cesium3DTileset::get_point_cloud_shading);
	ClassDB::bind_method(D_METHOD("refresh_point_cloud_shading"), &Cesium3DTileset::refresh_point_cloud_shading);
	ClassDB::bind_method(D_METHOD("_on_point_cloud_shading_changed"), &Cesium3DTileset::_on_point_cloud_shading_changed);
	ClassDB::bind_method(D_METHOD("has_active_tileset"), &Cesium3DTileset::has_active_tileset);
	ClassDB::bind_method(D_METHOD("get_tileset_bounds"), &Cesium3DTileset::get_tileset_bounds);
	ClassDB::bind_method(D_METHOD("get_tileset_source_aabb"), &Cesium3DTileset::get_tileset_source_aabb);
	ClassDB::bind_method(
		D_METHOD(
			"sample_height_most_detailed",
			"longitude_latitude_height"
		),
		&Cesium3DTileset::sample_height_most_detailed
	);
	ClassDB::bind_method(
		D_METHOD(
			"sample_height_most_detailed_exact",
			"longitude_latitude_height_components"
		),
		&Cesium3DTileset::sample_height_most_detailed_exact
	);
	ClassDB::bind_method(
		D_METHOD("get_streaming_statistics"),
		&Cesium3DTileset::get_streaming_statistics
	);
	ClassDB::bind_method(
		D_METHOD("set_debug_tile_state_capture_enabled", "enabled"),
		&Cesium3DTileset::set_debug_tile_state_capture_enabled
	);
	ClassDB::bind_method(
		D_METHOD("get_debug_tile_state_capture_enabled"),
		&Cesium3DTileset::get_debug_tile_state_capture_enabled
	);
	ClassDB::bind_method(
		D_METHOD("set_debug_tile_state_limit", "limit"),
		&Cesium3DTileset::set_debug_tile_state_limit
	);
	ClassDB::bind_method(
		D_METHOD("get_debug_tile_state_limit"),
		&Cesium3DTileset::get_debug_tile_state_limit
	);
	ClassDB::bind_method(
		D_METHOD("get_debug_tile_states"),
		&Cesium3DTileset::get_debug_tile_states
	);
	ClassDB::bind_method(
		D_METHOD("get_debug_tile_state_frame_number"),
		&Cesium3DTileset::get_debug_tile_state_frame_number
	);
	ClassDB::bind_method(
		D_METHOD("get_debug_tile_states_truncated"),
		&Cesium3DTileset::get_debug_tile_states_truncated
	);
	ADD_PROPERTY(
		PropertyInfo(Variant::BOOL, "debug_tile_state_capture_enabled"),
		"set_debug_tile_state_capture_enabled",
		"get_debug_tile_state_capture_enabled"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"debug_tile_state_limit",
			PROPERTY_HINT_RANGE,
			"1,4096,1"
		),
		"set_debug_tile_state_limit",
		"get_debug_tile_state_limit"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::ARRAY,
			"debug_tile_states",
			PROPERTY_HINT_ARRAY_TYPE,
			"CesiumTileDebugState",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_debug_tile_states"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::INT,
			"debug_tile_state_frame_number",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_debug_tile_state_frame_number"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::BOOL,
			"debug_tile_states_truncated",
			PROPERTY_HINT_NONE,
			"",
			PROPERTY_USAGE_NONE
		),
		"",
		"get_debug_tile_states_truncated"
	);
	ClassDB::bind_method(
		D_METHOD("_emit_load_failure_deferred", "failure"),
		&Cesium3DTileset::emit_load_failure_deferred
	);
	ADD_SIGNAL(MethodInfo(
		"load_failure",
		PropertyInfo(
			Variant::OBJECT,
			"failure",
			PROPERTY_HINT_RESOURCE_TYPE,
			"CesiumLoadFailure"
		)
	));
	ClassDB::bind_method(
		D_METHOD(
			"attach_raster_overlay",
			"primitive",
			"overlay_key",
			"texture",
			"texture_coordinate_id",
			"texture_coordinate_index",
			"translation",
			"scale"
		),
		&Cesium3DTileset::attach_raster_overlay
	);
	ClassDB::bind_method(
		D_METHOD("detach_raster_overlay", "primitive", "overlay_key", "expected_texture"),
		&Cesium3DTileset::detach_raster_overlay,
		DEFVAL(Ref<Texture2D>())
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"lifecycle_event_receiver",
			PROPERTY_HINT_NODE_TYPE,
			"Cesium3DTilesetLifecycleEventReceiver"
		),
		"set_lifecycle_event_receiver",
		"get_lifecycle_event_receiver"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"metadata_style",
			PROPERTY_HINT_RESOURCE_TYPE,
			"CesiumMetadataStyle"
		),
		"set_metadata_style",
		"get_metadata_style"
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::OBJECT,
			"point_cloud_shading",
			PROPERTY_HINT_RESOURCE_TYPE,
			"CesiumPointCloudShading"
		),
		"set_point_cloud_shading",
		"get_point_cloud_shading"
	);

	ClassDB::bind_method(D_METHOD("get_data_source"), &Cesium3DTileset::get_data_source);
	ClassDB::bind_method(D_METHOD("set_data_source", "data_source"), &Cesium3DTileset::set_data_source);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "data_source", PROPERTY_HINT_ENUM, "From Cesium Ion,From Url"), "set_data_source", "get_data_source");
	ClassDB::bind_integer_constant(get_class_static(), "CesiumDataSource", "FromCesiumIon", static_cast<int32_t>(CesiumDataSource::FromCesiumIon));
	ClassDB::bind_integer_constant(get_class_static(), "CesiumDataSource", "FromUrl", static_cast<int32_t>(CesiumDataSource::FromUrl));
	

	ClassDB::bind_method(D_METHOD("set_url", URL_P_NAME), &Cesium3DTileset::set_url);
	
	ClassDB::bind_method(D_METHOD("get_url"), &Cesium3DTileset::get_url);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");

	ClassDB::bind_method(
		D_METHOD("set_request_headers", "headers"),
		&Cesium3DTileset::set_request_headers
	);
	ClassDB::bind_method(
		D_METHOD("get_request_headers"),
		&Cesium3DTileset::get_request_headers
	);
	ADD_PROPERTY(
		PropertyInfo(
			Variant::DICTIONARY,
			"request_headers",
			PROPERTY_HINT_NONE,
			REQUEST_HEADERS_DESC
		),
		"set_request_headers",
		"get_request_headers"
	);

	ClassDB::bind_method(D_METHOD("set_ion_asset_id", ION_ASSET_ID_P_NAME), &Cesium3DTileset::set_ion_asset_id);
	ClassDB::bind_method(D_METHOD("get_ion_asset_id"), &Cesium3DTileset::get_ion_asset_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ion_asset_id"), "set_ion_asset_id", "get_ion_asset_id");


	ClassDB::bind_method(D_METHOD("set_show_hierarchy", "showHierarchy"), &Cesium3DTileset::set_show_hierarchy);
	ClassDB::bind_method(D_METHOD("get_show_hierarchy"), &Cesium3DTileset::get_show_hierarchy);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_hierarchy"), "set_show_hierarchy", "get_show_hierarchy");
	
#pragma endregion

#pragma region Public methods
	ClassDB::bind_method(D_METHOD("is_initial_loading_finished"), &Cesium3DTileset::is_initial_loading_finished);
	ClassDB::bind_method(D_METHOD("update_tileset", "camera_transform"), &Cesium3DTileset::update_tileset);
	ClassDB::bind_method(D_METHOD("set_debug_bounding_volumes_func", "onTileDrawn"), &Cesium3DTileset::set_debug_boundig_volumes_func);
	ClassDB::bind_method(D_METHOD("free_tile"), &Cesium3DTileset::free_tile);
	ClassDB::bind_method(D_METHOD("set_for_each_tile_func", "onTileFunc"), &Cesium3DTileset::set_for_each_tile_func);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "tileset_bounds", PROPERTY_HINT_RESOURCE_TYPE, "CesiumBoundingVolume", PROPERTY_USAGE_NONE), "", "get_tileset_bounds");
#pragma endregion

	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "Box", static_cast<int32_t>(EBoundingType::Box));
	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "CellVolume", static_cast<int32_t>(EBoundingType::CellVolume));
	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "CylinderRegion", static_cast<int32_t>(EBoundingType::CylinderRegion));
	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "Region", static_cast<int32_t>(EBoundingType::Region));
	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "RegionWithLooseFittingHeights", static_cast<int32_t>(EBoundingType::RegionWithLooseFittingHeights));
	ClassDB::bind_integer_constant(get_class_static(), "BoundingType", "Sphere", static_cast<int32_t>(EBoundingType::Sphere));

}

void Cesium3DTileset::_get_property_list(List<PropertyInfo>* properties) const
{
	#if defined(CESIUM_GD_MODULE)
	for (int32_t i = 0; i < properties->size(); i++) {
		PropertyInfo& propertyRef = properties->get(i);
	#elif defined(CESIUM_GD_EXT)
	for (auto it = properties->begin(); it != properties->end(); ++it) {
		PropertyInfo& propertyRef = *it;
	#endif
		propertyRef.usage = this->update_property_usage_flags(propertyRef);
	}
}


uint32_t Cesium3DTileset::update_property_usage_flags(const PropertyInfo& propertyRef) const
{	
	const String urlNameProp = "url";
	const String assetIdNameProp = "ion_asset_id";
	
	if (propertyRef.name == urlNameProp) {
		return this->m_selectedDataSource == CesiumDataSource::FromCesiumIon ? PROPERTY_USAGE_READ_ONLY : PROPERTY_USAGE_DEFAULT;
	}
	if (propertyRef.name == assetIdNameProp) {
		return this->m_selectedDataSource == CesiumDataSource::FromCesiumIon ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_READ_ONLY;
	}
	return propertyRef.usage;
}

bool Cesium3DTileset::_set(const StringName& p_name, const Variant& p_property)
{
	if (p_name == StringName(URL_P_NAME)) {
		this->set_url(p_property);
		return true;
	}
	if (p_name == StringName(ION_ASSET_ID_P_NAME)) {
		this->set_ion_asset_id(p_property);
		return true;
	}
	return false;
}

bool Cesium3DTileset::_get(const StringName& p_name, Variant& r_property) const
{
	if (p_name == StringName(URL_P_NAME)) {
		r_property = this->get_url();
		return true;
	}
	if (p_name == StringName(ION_ASSET_ID_P_NAME)) {
		r_property = this->get_ion_asset_id();
		return true;
	}

	return false;
}


void Cesium3DTileset::_ready() {
	this->capture_hardware_capabilities();
	if (this->m_automaticHardwareBudgetsEnabled) {
		this->apply_automatic_cache_budget();
	}
	if (!is_editor_mode()) return;
	Node* root = this->get_tree()->get_root();
	Camera3D* foundCamera = Godot3DTiles::AssetManipulation::find_georef_cam(root);
	if (foundCamera == nullptr) {
		WARN_PRINT("Could not find a Cesium Dynamic camera, try adding it manually in the Cesium Ion Panel");
		return;
	}
	Godot3DTiles::AssetManipulation::update_camera_tilesets(foundCamera);
}

void Cesium3DTileset::_exit_tree() {
	// Stop Cesium while its realized Godot tile children can still be safely
	// inspected by lifecycle receivers. The destructor keeps a reset fallback
	// for instances that never entered the scene tree.
	this->reset_movement_prediction();
	this->cancel_height_requests(
		"Tileset left the scene tree before height sampling completed."
	);
	this->m_tilesetBounds.unref();
	if (this->m_georeference != nullptr) {
		this->m_georeference->unregister_tileset_to_move_origin(this);
	}
	this->release_active_tileset();
}


CesiumGeoreference* Cesium3DTileset::get_georeference_node() const {
	return this->m_georeference;
}

void Cesium3DTileset::_enter_tree() {
	if (!is_editor_mode()) {
		// TODO: Replace this, it's bad code lol
		this->is_georeferenced(&this->m_georeference);
		return;
	}
	CesiumGeoreference* globe = Godot3DTiles::AssetManipulation::find_or_create_globe(this);
	if (globe == nullptr) {
		return;
	}
	//Parent to the globe
	this->m_georeference = globe;
	this->reparent(globe, true);
	this->set_owner(globe->get_parent_node_3d());
}
