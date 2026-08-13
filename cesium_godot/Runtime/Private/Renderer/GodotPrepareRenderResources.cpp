/*
 * Godot adaptation of Cesium for Unreal v2.29.0:
 * Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp
 */
#include "Runtime/Private/Renderer/GodotPrepareRenderResources.h"
#include "Runtime/Private/Diagnostics/CesiumLoadFailureQueue.h"
#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"
#include "CesiumImage/ImageAsset.h"
#include "CesiumImage/ImageDecoder.h"
#include "Models/Cesium3DTile.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/Metadata/CesiumMetadataStyle.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "Runtime/Private/RasterOverlays/CesiumRasterOverlayRendererOptions.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"
#include "Runtime/Public/Renderer/CesiumGltfInstancedComponent.h"
#include "Runtime/Private/Bounds/CesiumBoundingVolumeSnapshot.h"
#include "Runtime/Private/Metadata/CesiumGodotMetadataConversions.h"
#include "Godot/Nodes/CesiumGeoreference.h"
#include "Utils/CesiumVariantHash.h"
#include "error_names.hpp"
#include "glm/ext/vector_double3.hpp"
#include "glm/fwd.hpp"
#include "godot_cpp/core/error_macros.hpp"

#if defined(CESIUM_GD_EXT)
#include <godot_cpp/classes/mesh_instance3d.hpp>
#elif defined(CESIUM_GD_MODULE)
#include "scene/3d/mesh_instance_3d.h"
using namespace godot;
#endif

#include "CesiumAsync/AsyncSystem.h"
#include "Cesium3DTilesSelection/Tile.h"
#include "Runtime/Private/Renderer/CesiumGDModelLoader.h"
#include "Runtime/Private/Renderer/CesiumGDTextureLoader.h"
#include "CesiumRasterOverlays/RasterOverlayTile.h"
#include "CesiumRasterOverlays/RasterOverlay.h"
#include "Utils/CesiumMathUtils.h"
#include "CesiumGltf/ExtensionModelExtStructuralMetadata.h"
#include "Godot/Nodes/CesiumGDTileset.h"

#include <memory>
#include <chrono>

using namespace CesiumAsync;
using namespace Cesium3DTilesSelection;

namespace {
	Array primitive_features_to_array(
		const std::vector<Ref<CesiumPrimitiveFeatures>>& features
	) {
		Array result;
		result.resize(static_cast<int64_t>(features.size()));
		for (size_t index = 0; index < features.size(); ++index) {
			result[static_cast<int64_t>(index)] = features[index];
		}
		return result;
	}

Ref<CesiumBoundingVolume> make_bounding_volume_resource(
	const Cesium3DTilesSelection::BoundingVolume& source
) {
	Ref<CesiumBoundingVolume> result;
	result.instantiate();
	result->initialize(create_cesium_bounding_volume_snapshot(source));
	return result;
}

bool apply_shared_model_resource(
	CesiumGDPreparedModel& prepared,
	const std::shared_ptr<CesiumGltfSharedModelResource>& resource
) {
	if (
		resource == nullptr || resource->mesh.is_null() ||
		resource->primitives.size() != prepared.primitives.size() ||
		resource->geometryBytes != prepared.geometryBytes ||
		resource->textureBytes != prepared.textureBytes
	) {
		return false;
	}
	for (size_t index = 0; index < prepared.primitives.size(); ++index) {
		const CesiumGDPreparedPrimitive& target = prepared.primitives[index];
		const CesiumGltfSharedPrimitiveResource& source =
			resource->primitives[index];
		if (
			target.source.nodeIndex != source.nodeIndex ||
			target.source.meshIndex != source.meshIndex ||
			target.source.primitiveIndex != source.primitiveIndex ||
			target.isGpuInstanced != source.isGpuInstanced ||
			target.isTranslucent != source.isTranslucent ||
			target.instanceCount != source.instanceCount
		) {
			return false;
		}
	}

	prepared.realizedMesh = resource->mesh;
	prepared.realizedMetadata = resource->metadata;
	prepared.realizedPrimitiveFeatures.clear();
	prepared.realizedPrimitiveFeatures.reserve(
		static_cast<size_t>(resource->primitiveFeatures.size())
	);
	for (int64_t index = 0; index < resource->primitiveFeatures.size(); ++index) {
		Ref<CesiumPrimitiveFeatures> features =
			resource->primitiveFeatures[index];
		prepared.realizedPrimitiveFeatures.push_back(features);
	}
	for (size_t index = 0; index < prepared.primitives.size(); ++index) {
		CesiumGDPreparedPrimitive& target = prepared.primitives[index];
		const CesiumGltfSharedPrimitiveResource& source =
			resource->primitives[index];
		target.realizedSurfaceIndex = source.realizedSurfaceIndex;
		target.realizedMaterial = source.material;
		target.realizedMesh = source.primitiveMesh;
		target.realizedMultiMesh = source.multiMesh;
		target.realizedInstanceFeatures = source.instanceFeatures;
		target.realizedPrimitiveMetadata = source.primitiveMetadata;
	}
	prepared.nextPrimitiveToRealize = prepared.primitives.size();
	prepared.metadataRealizationComplete = true;
	prepared.sharedModelResource = resource;
	return true;
}

std::shared_ptr<CesiumGltfSharedModelResource> create_shared_model_resource(
	const CesiumGDPreparedModel& prepared
) {
	auto resource = std::make_shared<CesiumGltfSharedModelResource>();
	resource->mesh = prepared.realizedMesh;
	resource->metadata = prepared.realizedMetadata;
	resource->primitiveFeatures =
		primitive_features_to_array(prepared.realizedPrimitiveFeatures);
	resource->imageResources = prepared.sharedImageResources;
	resource->geometryBytes = prepared.geometryBytes;
	resource->textureBytes = prepared.textureBytes;
	resource->primitives.reserve(prepared.primitives.size());
	for (const CesiumGDPreparedPrimitive& primitive : prepared.primitives) {
		CesiumGltfSharedPrimitiveResource shared;
		shared.nodeIndex = primitive.source.nodeIndex;
		shared.meshIndex = primitive.source.meshIndex;
		shared.primitiveIndex = primitive.source.primitiveIndex;
		shared.isGpuInstanced = primitive.isGpuInstanced;
		shared.isTranslucent = primitive.isTranslucent;
		shared.instanceCount = primitive.instanceCount;
		shared.realizedSurfaceIndex = primitive.realizedSurfaceIndex;
		shared.material = primitive.realizedMaterial;
		shared.primitiveMesh = primitive.realizedMesh;
		shared.multiMesh = primitive.realizedMultiMesh;
		shared.instanceFeatures = primitive.realizedInstanceFeatures;
		shared.primitiveMetadata = primitive.realizedPrimitiveMetadata;
		resource->primitives.emplace_back(std::move(shared));
	}
	return resource;
}
}

GodotPrepareRenderResources::GodotPrepareRenderResources(
	Cesium3DTileset* source,
	const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
	const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue
) : m_statistics(statistics), m_failureQueue(failureQueue) {
	this->m_sharedImageCache =
		std::make_shared<CesiumGltfImageAssetResourceCache>(statistics);
	this->m_sharedModelCache =
		std::make_shared<CesiumGltfModelResourceCache>(statistics);
	if (source == nullptr) {
		return;
	}
	this->m_tileset = ObjectID(source->get_instance_id());
	this->m_createPhysicsMeshes = source->get_create_physics_meshes();
	CesiumGeoreference* georeference = nullptr;
	this->m_isGeoreferenced = source->is_georeferenced(&georeference);
	if (georeference != nullptr) {
		this->m_isTrueOrigin =
			georeference->get_origin_type_raw() ==
			CesiumGeoreference::OriginType::TrueOrigin;
	}
	this->m_maximumPrimitiveGeometryBytes = static_cast<uint64_t>(
		source->get_maximum_primitive_geometry_upload_bytes()
	);
	this->m_maximumPrimitiveTextureBytes = static_cast<uint64_t>(
		source->get_maximum_primitive_texture_upload_bytes()
	);
	this->m_enableLodTransitionDither = source->get_lod_transitions_enabled();
	this->m_enableTranslucencyDepthPrepass =
		source->get_translucency_depth_prepass_enabled();
	const Ref<CesiumMetadataStyle> metadataStyle = source->get_metadata_style();
	if (metadataStyle.is_valid()) {
		this->m_featureStyle = metadataStyle->get_encoding_description();
	}
}

void GodotPrepareRenderResources::release_generation_resources() {
	if (this->m_sharedImageCache != nullptr) {
		this->m_sharedImageCache->release_generation_resources();
	}
}

Cesium3DTileset* GodotPrepareRenderResources::resolve_tileset() const {
	if (this->m_tileset.is_null()) {
		return nullptr;
	}
	return Object::cast_to<Cesium3DTileset>(
		ObjectDB::get_instance(this->m_tileset)
	);
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> GodotPrepareRenderResources::prepareInLoadThread(const CesiumAsync::AsyncSystem& asyncSystem, Cesium3DTilesSelection::TileLoadResult&& tileLoadResult, const glm::dmat4& transform, const std::any& rendererOptions)
{
	CesiumGltf::Model* model = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);

	if (model == nullptr) {
		return asyncSystem.createResolvedFuture(TileLoadResultAndRenderResources{ std::move(tileLoadResult), nullptr });
	}

	const std::shared_ptr<CesiumTilesetRuntimeStatistics> statistics =
		this->m_statistics;
	const uint64_t maximumPrimitiveGeometryBytes =
		this->m_maximumPrimitiveGeometryBytes;
	const uint64_t maximumPrimitiveTextureBytes =
		this->m_maximumPrimitiveTextureBytes;
	const std::shared_ptr<CesiumGltfImageAssetResourceCache> sharedImageCache =
		this->m_sharedImageCache;
	const std::shared_ptr<CesiumGltfModelResourceCache> sharedModelCache =
		this->m_sharedModelCache;
	const std::shared_ptr<CesiumLoadFailureQueue> failureQueue =
		this->m_failureQueue;
	const bool enableLodTransitionDither = this->m_enableLodTransitionDither;
	const bool enableTranslucencyDepthPrepass =
		this->m_enableTranslucencyDepthPrepass;
	const CesiumFeatureStyleEncodingDescription featureStyle =
		this->m_featureStyle;
	const uint64_t sourceInstanceId = static_cast<uint64_t>(this->m_tileset);
	return asyncSystem.createFuture<TileLoadResultAndRenderResources>([
		statistics,
		maximumPrimitiveGeometryBytes,
		maximumPrimitiveTextureBytes,
		sharedImageCache,
		sharedModelCache,
		failureQueue,
		enableLodTransitionDither,
		enableTranslucencyDepthPrepass,
		featureStyle,
		sourceInstanceId,
		tileLoadResult = std::move(tileLoadResult)
	](Promise<TileLoadResultAndRenderResources> p_promise) mutable {
		const auto preparationStart = std::chrono::steady_clock::now();
		const std::string contentUrl = tileLoadResult.pCompletedRequest != nullptr
			? redact_cesium_diagnostic_url(
				tileLoadResult.pCompletedRequest->url()
			)
			: std::string();
		// Re-extract model pointer since tileLoadResult was moved
		CesiumGltf::Model* movedModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
		if (movedModel == nullptr) {
			ERR_PRINT("Model was invalidated after move!");
			if (failureQueue != nullptr) {
				CesiumLoadFailureRecord record;
				record.sourceInstanceId = sourceInstanceId;
				record.category = CesiumLoadFailure::Category::Renderer;
				record.stage = CesiumLoadFailure::Stage::RendererPreparation;
				record.message = "Model was invalidated before renderer preparation";
				record.url = contentUrl;
				failureQueue->push(std::move(record));
			}
			TileLoadResultAndRenderResources errorResult{ std::move(tileLoadResult), nullptr };
			p_promise.resolve(errorResult);
			return;
		}

		Error err;
		CesiumGDPreparedModel* prepared =
			CesiumGDModelLoader::prepare_model(
				*movedModel,
				&err,
				featureStyle
			);
		const uint64_t decodeMicroseconds = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - preparationStart
			).count()
		);
		if (statistics != nullptr) {
			statistics->decodeCount.fetch_add(1, std::memory_order_relaxed);
			statistics->decodeMicroseconds.fetch_add(
				decodeMicroseconds,
				std::memory_order_relaxed
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->decodeMaximumMicroseconds,
				decodeMicroseconds
			);
		}

		if (err != Error::OK || prepared == nullptr) {
			String errorMsg = String("Error generating meshes for tile ") + REFLECT_ERR_NAME(err);
			ERR_PRINT(errorMsg);
			if (statistics != nullptr) {
				statistics->decodeFailureCount.fetch_add(
					1,
					std::memory_order_relaxed
				);
			}
			if (failureQueue != nullptr) {
				CesiumLoadFailureRecord record;
				record.sourceInstanceId = sourceInstanceId;
				record.category = CesiumLoadFailure::Category::Decode;
				record.stage = CesiumLoadFailure::Stage::GltfDecode;
				record.message = "Could not decode glTF geometry (Godot error " +
					std::to_string(static_cast<int32_t>(err)) + ")";
				record.url = contentUrl;
				failureQueue->push(std::move(record));
			}
			// Always resolve with nullptr - reject() crashes with LIBASYNC_NO_EXCEPTIONS
			tileLoadResult.state = TileLoadResultState::Failed;
			TileLoadResultAndRenderResources errorResult{ std::move(tileLoadResult), nullptr };
			p_promise.resolve(errorResult);
			return;
		}
		prepared->sharedImageCache = sharedImageCache;
		prepared->sharedModelCache = sharedModelCache;
		prepared->enableLodTransitionDither = enableLodTransitionDither;
		prepared->enableTranslucencyDepthPrepass =
			enableTranslucencyDepthPrepass;
		if (tileLoadResult.pCompletedRequest != nullptr) {
			prepared->contentKey = tileLoadResult.pCompletedRequest->url();
		}
		if (statistics != nullptr) {
			const uint64_t elapsedMicroseconds = static_cast<uint64_t>(
				std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::steady_clock::now() - preparationStart
				).count()
			);
			statistics->workerPreparationCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
			statistics->workerPreparationMicroseconds.fetch_add(
				elapsedMicroseconds,
				std::memory_order_relaxed
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->workerPreparationMaximumMicroseconds,
				elapsedMicroseconds
			);
			statistics->preparedGeometryBytes.fetch_add(
				prepared->geometryBytes,
				std::memory_order_relaxed
			);
			statistics->preparedTextureBytes.fetch_add(
				prepared->textureBytes,
				std::memory_order_relaxed
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->maximumPreparedGeometryBytes,
				prepared->geometryBytes
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->maximumPreparedTextureBytes,
				prepared->textureBytes
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->maximumPreparedPrimitiveGeometryBytes,
				prepared->maximumPrimitiveGeometryBytes
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				statistics->maximumPreparedPrimitiveTextureBytes,
				prepared->maximumPrimitiveTextureBytes
			);
		}
		if (
			(maximumPrimitiveGeometryBytes > 0 &&
				prepared->maximumPrimitiveGeometryBytes >
					maximumPrimitiveGeometryBytes) ||
			(maximumPrimitiveTextureBytes > 0 &&
				prepared->maximumPrimitiveTextureBytes >
					maximumPrimitiveTextureBytes)
		) {
			ERR_PRINT(
				String("Rejecting oversized 3D Tiles leaf: primitive geometry=") +
				String::num_uint64(prepared->maximumPrimitiveGeometryBytes) +
				" bytes, primitive textures=" +
				String::num_uint64(prepared->maximumPrimitiveTextureBytes) +
				" bytes. Retile or resize the source payload."
			);
			if (failureQueue != nullptr) {
				CesiumLoadFailureRecord record;
				record.sourceInstanceId = sourceInstanceId;
				record.category = CesiumLoadFailure::Category::Renderer;
				record.stage = CesiumLoadFailure::Stage::RendererPreparation;
				record.message = "Prepared primitive exceeds the configured upload cap";
				record.url = contentUrl;
				failureQueue->push(std::move(record));
			}
			if (statistics != nullptr) {
				statistics->oversizedPayloadRejectionCount.fetch_add(
					1,
					std::memory_order_relaxed
				);
			}
			delete prepared;
			tileLoadResult.state = TileLoadResultState::Failed;
			TileLoadResultAndRenderResources errorResult{
				std::move(tileLoadResult),
				nullptr
			};
			p_promise.resolve(errorResult);
			return;
		}
		TileLoadResultAndRenderResources result{
			std::move(tileLoadResult),
			static_cast<void*>(prepared)
		};

		p_promise.resolve(result);

	});
}

void* GodotPrepareRenderResources::prepareInMainThread(Tile& tile, void* pLoadThreadResult)
{
	while (true) {
		MainThreadRendererResourcesPreparationResult result =
			this->prepareInMainThreadIncrementally(
				tile,
				pLoadThreadResult,
				0.0
			);
		if (result.isComplete) {
			return result.pRenderResources;
		}
	}
}

MainThreadRendererResourcesPreparationResult
GodotPrepareRenderResources::prepareInMainThreadIncrementally(
	Tile& tile,
	void* pLoadThreadResult,
	double timeBudgetMilliseconds
) {
	if (pLoadThreadResult == nullptr) {
		return {nullptr, true};
	}
	Cesium3DTileset* tileset = this->resolve_tileset();
	if (tileset == nullptr) {
		delete static_cast<CesiumGDPreparedModel*>(pLoadThreadResult);
		return {nullptr, true};
	}
	const TileRenderContent* pRenderContent =
		tile.getContent().getRenderContent();
	if (pRenderContent == nullptr) {
		delete static_cast<CesiumGDPreparedModel*>(pLoadThreadResult);
		return {nullptr, true};
	}

	auto* prepared = static_cast<CesiumGDPreparedModel*>(pLoadThreadResult);
	const CesiumGltf::Model& model = pRenderContent->getModel();
	const auto invocationStart = std::chrono::steady_clock::now();
	auto recordStep = [this](
		CesiumGDPreparedModel& target,
		const std::chrono::steady_clock::time_point& stepStart
	) {
		const uint64_t elapsedMicroseconds = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::steady_clock::now() - stepStart
			).count()
		);
		target.realizationMicroseconds += elapsedMicroseconds;
		if (this->m_statistics != nullptr) {
			this->m_statistics->mainThreadRealizationStepCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
			this->m_statistics->mainThreadRealizationStepMicroseconds.fetch_add(
				elapsedMicroseconds,
				std::memory_order_relaxed
			);
			CesiumTilesetRuntimeStatistics::update_maximum(
				this->m_statistics->mainThreadRealizationStepMaximumMicroseconds,
				elapsedMicroseconds
			);
		}
	};

	bool complete = false;
	if (!prepared->sharedModelLookupComplete) {
		prepared->sharedModelLookupComplete = true;
		if (
			prepared->sharedModelCache != nullptr &&
			!prepared->contentKey.empty()
		) {
			std::shared_ptr<CesiumGltfSharedModelResource> shared =
				prepared->sharedModelCache->acquire(prepared->contentKey);
			if (
				shared != nullptr &&
				!apply_shared_model_resource(*prepared, shared)
			) {
				WARN_PRINT(
					"Resolved glTF content identity changed within one tileset "
					"generation; realizing an independent renderer resource"
				);
			} else if (shared != nullptr) {
				complete = true;
			}
		}
	}
	auto finalStepStart = std::chrono::steady_clock::now();
	while (!complete) {
		const auto stepStart = std::chrono::steady_clock::now();
		finalStepStart = stepStart;
		Error error = Error::OK;
		complete = CesiumGDModelLoader::realize_prepared_model_incrementally(
			model,
			*prepared,
			&error
		);
		if (prepared->pendingMaterialWarningError.has_value()) {
			if (this->m_failureQueue != nullptr) {
				CesiumLoadFailureRecord failure;
				failure.sourceInstanceId = static_cast<uint64_t>(this->m_tileset);
				failure.category = CesiumLoadFailure::Category::Material;
				failure.stage = CesiumLoadFailure::Stage::MaterialCreation;
				failure.terminal = false;
				failure.message =
					"A glTF material property was invalid or unsupported; valid "
					"properties continue with fallback (Godot error " +
					std::to_string(*prepared->pendingMaterialWarningError) + ")";
				failure.url = redact_cesium_diagnostic_url(prepared->contentKey);
				this->m_failureQueue->push(std::move(failure));
			}
			prepared->pendingMaterialWarningError.reset();
		}
		if (error != Error::OK) {
			recordStep(*prepared, stepStart);
			ERR_PRINT(
				String("Error realizing meshes for tile ") +
				REFLECT_ERR_NAME(error)
			);
			if (this->m_failureQueue != nullptr) {
				CesiumLoadFailureRecord failure;
				failure.sourceInstanceId = static_cast<uint64_t>(this->m_tileset);
				failure.category = prepared->realizationFailureStage ==
					CesiumGDRealizationFailureStage::Material
					? CesiumLoadFailure::Category::Material
					: CesiumLoadFailure::Category::Renderer;
				failure.stage = prepared->realizationFailureStage ==
					CesiumGDRealizationFailureStage::Material
					? CesiumLoadFailure::Stage::MaterialCreation
					: CesiumLoadFailure::Stage::RendererPreparation;
				failure.message = "Could not realize prepared glTF (Godot error " +
					std::to_string(static_cast<int32_t>(error)) + ")";
				failure.url = redact_cesium_diagnostic_url(prepared->contentKey);
				this->m_failureQueue->push(std::move(failure));
			}
			delete prepared;
			return {nullptr, true};
		}
		if (complete) {
			break;
		}
		recordStep(*prepared, stepStart);
		if (
			timeBudgetMilliseconds > 0.0 &&
			std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - invocationStart
			).count() >= timeBudgetMilliseconds
		) {
			if (this->m_statistics != nullptr) {
				this->m_statistics->incrementalRealizationYieldCount.fetch_add(
					1,
					std::memory_order_relaxed
				);
			}
			return {nullptr, false};
		}
	}

	std::unique_ptr<CesiumGDPreparedModel> completed(prepared);
	Ref<ArrayMesh> meshData = completed->realizedMesh;
	if (meshData.is_null()) {
		recordStep(*completed, finalStepStart);
		return {nullptr, true};
	}
	if (
		completed->sharedModelResource == nullptr &&
		completed->sharedModelCache != nullptr &&
		!completed->contentKey.empty()
	) {
		std::shared_ptr<CesiumGltfSharedModelResource> created =
			create_shared_model_resource(*completed);
		std::shared_ptr<CesiumGltfSharedModelResource> published =
			completed->sharedModelCache->publish(
				completed->contentKey,
				created
			);
		if (published != created) {
			if (!apply_shared_model_resource(*completed, published)) {
				WARN_PRINT(
					"Could not apply an existing resolved glTF renderer resource"
				);
			} else {
				meshData = completed->realizedMesh;
			}
		} else {
			completed->sharedModelResource = std::move(created);
		}
	}
	Cesium3DTile* instance = memnew(Cesium3DTile);
	instance->set_mesh(meshData);
	instance->set_transform(Transform3D());
	std::vector<GeometryInstance3D*> primitiveRenderNodes;
	std::vector<int32_t> primitiveMeshSurfaceIndices;
	std::vector<int32_t> parentSurfaceToPrimitiveIndex(
		static_cast<size_t>(meshData->get_surface_count()),
		-1
	);
	std::vector<CesiumTileCollisionPrimitive> collisionPrimitives;
	primitiveRenderNodes.reserve(completed->primitives.size());
	primitiveMeshSurfaceIndices.reserve(completed->primitives.size());
	for (size_t primitiveIndex = 0;
		primitiveIndex < completed->primitives.size();
		++primitiveIndex) {
		const CesiumGDPreparedPrimitive& primitive =
			completed->primitives[primitiveIndex];
		if (
			!primitive.isGpuInstanced && primitive.realizedMesh.is_valid() &&
			primitive.realizedSurfaceIndex >= 0 &&
			primitive.realizedSurfaceIndex <
				primitive.realizedMesh->get_surface_count() &&
			primitive.realizedMesh->surface_get_primitive_type(
				primitive.realizedSurfaceIndex
			) == Mesh::PRIMITIVE_TRIANGLES
		) {
			collisionPrimitives.push_back({
				static_cast<int32_t>(primitiveIndex),
				primitive.realizedMesh,
				primitive.realizedSurfaceIndex
			});
		}
		if (primitive.isGpuInstanced) {
			CesiumGltfInstancedComponent* instanced =
				memnew(CesiumGltfInstancedComponent);
			instanced->set_name(
				"InstancedPrimitive" + String::num_int64(primitiveIndex)
			);
			instanced->initialize(
				static_cast<int32_t>(primitiveIndex),
				primitive.realizedMultiMesh,
				primitive.instanceBounds,
				CesiumMathUtils::from_glm_mat4(primitive.source.transform),
				primitive.realizedInstanceFeatures
			);
			instance->add_child(instanced, true);
			if (primitive.isTranslucent) {
				instanced->set_sorting_use_aabb_center(true);
			}
			primitiveRenderNodes.push_back(instanced);
			primitiveMeshSurfaceIndices.push_back(0);
		} else if (primitive.isTranslucent) {
			MeshInstance3D* translucent = memnew(MeshInstance3D);
			translucent->set_name(
				"TranslucentPrimitive" + String::num_int64(primitiveIndex)
			);
			translucent->set_mesh(primitive.realizedMesh);
			// Godot sorts transparent GeometryInstance3D nodes by origin unless
			// asked to use their bounds. One node per source BLEND primitive plus
			// the AABB center is the closest public-API counterpart to Unreal's
			// independently sorted primitive components.
			translucent->set_sorting_use_aabb_center(true);
			instance->add_child(translucent, true);
			primitiveRenderNodes.push_back(translucent);
			primitiveMeshSurfaceIndices.push_back(0);
		} else {
			primitiveRenderNodes.push_back(instance);
			primitiveMeshSurfaceIndices.push_back(
				primitive.realizedSurfaceIndex
			);
			if (
				primitive.realizedSurfaceIndex >= 0 &&
				primitive.realizedSurfaceIndex < meshData->get_surface_count()
			) {
				parentSurfaceToPrimitiveIndex[
					static_cast<size_t>(primitive.realizedSurfaceIndex)
				] = static_cast<int32_t>(primitiveIndex);
			}
		}
	}
	instance->set_primitive_render_nodes(
		primitiveRenderNodes,
		primitiveMeshSurfaceIndices,
		parentSurfaceToPrimitiveIndex
	);
	instance->set_original_position(glm::dvec3(0.0));
	if (this->m_createPhysicsMeshes && !collisionPrimitives.empty()) {
		instance->generate_tile_collision_from_primitives(collisionPrimitives);
	}
	instance->set_model_metadata(completed->realizedMetadata);
	instance->set_primitive_features(
		primitive_features_to_array(completed->realizedPrimitiveFeatures)
	);
	if (completed->sharedModelResource != nullptr) {
		instance->set_shared_model_resource(completed->sharedModelResource);
	} else {
		instance->set_shared_image_resources(
			std::move(completed->sharedImageResources)
		);
	}
	instance->set_tileset_no_reparent(tileset);
	const std::string tileId =
		TileIdUtilities::createTileIdString(tile.getTileID());
	instance->set_tile_id(String(tileId.c_str()));
	instance->set_tile_extras(
		CesiumGodotMetadataConversions::json_value_to_variant(tile.getExtras())
	);
	Ref<CesiumBoundingVolume> contentBounds;
	if (tile.getContentBoundingVolume()) {
		contentBounds = make_bounding_volume_resource(
			*tile.getContentBoundingVolume()
		);
	}
	Ref<CesiumBoundingVolume> viewerRequestBounds;
	if (tile.getViewerRequestVolume()) {
		viewerRequestBounds = make_bounding_volume_resource(
			*tile.getViewerRequestVolume()
		);
	}
	instance->set_bounding_volumes(
		make_bounding_volume_resource(tile.getBoundingVolume()),
		contentBounds,
		viewerRequestBounds
	);
	const glm::dmat4& tileTransform = tile.getTransform();
	instance->set_anchor_to_ecef_transform(tileTransform);
	if (this->m_isTrueOrigin) {
		CesiumGeoreference* georeference = tileset->get_georeference_node();
		const Transform3D localTransform = CesiumMathUtils::from_glm_mat4(
			georeference != nullptr
				? georeference->ecef_transform_to_local(tileTransform)
				: tileTransform
		);
		const Transform3D absoluteTransform = georeference != nullptr
			? georeference->get_global_transform() * localTransform
			: tileset->get_global_transform() * localTransform;
		instance->set_original_position(
			CesiumMathUtils::to_glm_dvec3(absoluteTransform.origin)
		);
		instance->set_original_basis(absoluteTransform.basis);
	} else {
		const glm::dvec3 position = glm::dvec3(
			CesiumMathUtils::ecef_to_engine(tileTransform[3])
		);
		instance->set_original_position(
			instance->get_original_position() + position
		);
		const glm::dmat4 fixedToGodot(
			glm::dvec4(1.0, 0.0, 0.0, 0.0),
			glm::dvec4(0.0, 0.0, -1.0, 0.0),
			glm::dvec4(0.0, 1.0, 0.0, 0.0),
			glm::dvec4(0.0, 0.0, 0.0, 1.0)
		);
		instance->set_original_basis(
			CesiumMathUtils::from_glm_mat4(fixedToGodot * tileTransform).basis
		);
	}
	instance->hide();
	tileset->finalize_loaded_tile(
		instance,
		model,
		*completed,
		tile.getGeometricError(),
		tile.getRefine() == Cesium3DTilesSelection::TileRefine::Add
	);
	recordStep(*completed, finalStepStart);

	if (this->m_statistics != nullptr) {
		this->m_statistics->mainThreadRealizationCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		this->m_statistics->mainThreadRealizationMicroseconds.fetch_add(
			completed->realizationMicroseconds,
			std::memory_order_relaxed
		);
		CesiumTilesetRuntimeStatistics::update_maximum(
			this->m_statistics->mainThreadRealizationMaximumMicroseconds,
			completed->realizationMicroseconds
		);
	}
	return {instance, true};
}

void GodotPrepareRenderResources::free(Tile& tile, void* pLoadThreadResult, void* pMainThreadResult) noexcept
{
	const auto& tileId = tile.getTileID();
	uint64_t hash = static_cast<uint64_t>(std::visit(CesiumVariantHash{}, tileId));
	if (pMainThreadResult != nullptr) {
		auto* instance = static_cast<Cesium3DTile*>(pMainThreadResult);
		Cesium3DTileset* tileset = this->resolve_tileset();
		if (tileset != nullptr) {
			tileset->notify_tile_unloading(instance);
			// Native invokes renderer-resource release on the main thread. Queue
			// the tile itself for deletion immediately instead of deferring the
			// request through the tileset: the tileset may be destroyed before a
			// deferred method call runs, which would strand the tile's GPU resources.
			tileset->free_tile(instance, hash);
		} else if (instance->is_inside_tree()) {
			instance->queue_free();
		} else {
			memdelete(instance);
		}
		if (this->m_statistics != nullptr) {
			this->m_statistics->tileUnloadCount.fetch_add(
				1,
				std::memory_order_relaxed
			);
		}
		return;
	}

	// A tile may be evicted after worker preparation but before main-thread
	// realization. It was never reported as loaded, so only release its node.
	if (pLoadThreadResult != nullptr) {
		delete static_cast<CesiumGDPreparedModel*>(pLoadThreadResult);
	}
}

void GodotPrepareRenderResources::attachRasterInMainThread(const Tile& tile, int32_t overlayTextureCoordinateID, const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pMainThreadRendererResources, const glm::dvec2& translation, const glm::dvec2& scale)
{
	Cesium3DTileset* tileset = this->resolve_tileset();
	if (tileset == nullptr) {
		return;
	}
	const Cesium3DTilesSelection::TileContent& content = tile.getContent();
	const Cesium3DTilesSelection::TileRenderContent* renderContent =
		content.getRenderContent();
	if (renderContent == nullptr || pMainThreadRendererResources == nullptr) {
		return;
	}
	void* rawRenderResources = renderContent->getRenderResources();
	auto* meshInstance = static_cast<Cesium3DTile*>(rawRenderResources);
	if (meshInstance == nullptr) {
		return;
	}

	Ref<ImageTexture> godotTexture = static_cast<ImageTexture*>(pMainThreadRendererResources);
	if (godotTexture.is_null()) {
		return;
	}

	const String overlayKey(rasterTile.getOverlay().getName().c_str());
	const String overlaySemantic =
		"_CESIUMOVERLAY_" + String::num_int64(overlayTextureCoordinateID);
	const Vector2 translationOffsets = CesiumMathUtils::from_glm_vec2(translation);
	const Vector2 scaleFactors = CesiumMathUtils::from_glm_vec2(scale);

	for (int32_t surfaceIndex = 0;
		surfaceIndex < meshInstance->get_loaded_tile_primitive_count();
		++surfaceIndex) {
		Ref<CesiumLoadedTilePrimitive> primitive =
			meshInstance->get_loaded_tile_primitive(surfaceIndex);
		if (
			primitive.is_null() ||
			!primitive->get_attributes().has(overlaySemantic)
		) {
			continue;
		}

		tileset->attach_raster_overlay(
			primitive,
			overlayKey,
			godotTexture,
			overlayTextureCoordinateID,
			primitive->get_overlay_texture_coordinate_index(
				overlayTextureCoordinateID
			),
			translationOffsets,
			scaleFactors
		);
	}
}

void GodotPrepareRenderResources::detachRasterInMainThread(const Tile& tile, int32_t overlayTextureCoordinateID, const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pMainThreadRendererResources) noexcept
{
	Cesium3DTileset* tileset = this->resolve_tileset();
	if (tileset == nullptr) {
		return;
	}
	const Cesium3DTilesSelection::TileContent& content = tile.getContent();
	const Cesium3DTilesSelection::TileRenderContent* renderContent =
		content.getRenderContent();
	if (renderContent == nullptr || pMainThreadRendererResources == nullptr) {
		return;
	}

	auto* meshInstance = static_cast<Cesium3DTile*>(
		renderContent->getRenderResources()
	);
	if (meshInstance == nullptr) {
		return;
	}

	Ref<ImageTexture> expectedTexture =
		static_cast<ImageTexture*>(pMainThreadRendererResources);
	const String overlayKey(rasterTile.getOverlay().getName().c_str());
	for (int32_t surfaceIndex = 0;
		surfaceIndex < meshInstance->get_loaded_tile_primitive_count();
		++surfaceIndex) {
		Ref<CesiumLoadedTilePrimitive> primitive =
			meshInstance->get_loaded_tile_primitive(surfaceIndex);
		tileset->detach_raster_overlay(
			primitive,
			overlayKey,
			expectedTexture
		);
	}
}

void* GodotPrepareRenderResources::prepareRasterInLoadThread(CesiumImage::ImageAsset& image, const std::any& rendererOptions)
{
	const CesiumRasterOverlayRendererOptions* godotOptions =
		std::any_cast<CesiumRasterOverlayRendererOptions>(&rendererOptions);
	if (godotOptions != nullptr && !godotOptions->generateMipmaps) {
		return nullptr;
	}
	const std::optional<std::string> error =
		CesiumImage::ImageDecoder::generateMipMaps(image);
	if (error && this->m_failureQueue != nullptr) {
		CesiumLoadFailureRecord failure;
		failure.sourceInstanceId = static_cast<uint64_t>(this->m_tileset);
		failure.category = CesiumLoadFailure::Category::Decode;
		failure.stage = CesiumLoadFailure::Stage::RasterTileRequest;
		failure.terminal = false;
		failure.message = *error;
		this->m_failureQueue->push(std::move(failure));
	}
	return nullptr;
}

void* GodotPrepareRenderResources::prepareRasterInMainThread(CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pLoadThreadResult)
{
	CesiumImage::ImageAsset& imageCesium = *rasterTile.getImage().get();
	Ref<ImageTexture> godotTexture =
		CesiumGDTextureLoader::load_image_texture(imageCesium, false);
	if (godotTexture.is_null()) {
		if (this->m_failureQueue != nullptr) {
			CesiumLoadFailureRecord failure;
			failure.sourceInstanceId = static_cast<uint64_t>(this->m_tileset);
			failure.category = CesiumLoadFailure::Category::Renderer;
			failure.stage = CesiumLoadFailure::Stage::TextureUpload;
			failure.message = "Could not create a Godot texture for raster imagery";
			failure.overlayKey = rasterTile.getOverlay().getName();
			this->m_failureQueue->push(std::move(failure));
		}
		return nullptr;
	}
	const uint64_t releasedBytes =
		CesiumGDTextureLoader::release_pixel_data(imageCesium);
	if (releasedBytes > 0 && this->m_statistics != nullptr) {
		this->m_statistics->releasedCpuTextureCount.fetch_add(
			1,
			std::memory_order_relaxed
		);
		this->m_statistics->releasedCpuTextureBytes.fetch_add(
			releasedBytes,
			std::memory_order_relaxed
		);
	}
	godotTexture->reference();
	return static_cast<void*>(godotTexture.ptr());
}

void GodotPrepareRenderResources::freeRaster(const CesiumRasterOverlays::RasterOverlayTile& rasterTile, void* pLoadThreadResult, void* pMainThreadResult) noexcept
{
	auto* rasterMainThreadTexture = static_cast<ImageTexture*>(pMainThreadResult);
	if (rasterMainThreadTexture == nullptr) return;
	// prepareRasterInMainThread transfers one explicit reference through the
	// Native void* renderer-resource slot. RefCounted::unreference reports when
	// that was the final reference, but raw callers must then destroy the object
	// themselves (Ref<T> normally performs this second step in its destructor).
	if (rasterMainThreadTexture->unreference()) {
		memdelete(rasterMainThreadTexture);
	}
}
