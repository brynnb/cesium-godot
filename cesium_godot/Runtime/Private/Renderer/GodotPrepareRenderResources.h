/*
 * Godot adaptation of Cesium for Unreal v2.29.0:
 * - Source/CesiumRuntime/Private/UnrealPrepareRendererResources.h
 * - Source/CesiumRuntime/Private/UnrealPrepareRendererResources.cpp
 *
 * Last upstream review: Cesium for Unreal v2.29.0.
 */
#ifndef GODOT_PREPARE_RENDER_RESOURCES_H
#define GODOT_PREPARE_RENDER_RESOURCES_H

#include "Cesium3DTilesSelection/IPrepareRendererResources.h"
#include "CesiumImage/ImageAsset.h"
#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Runtime/Private/Renderer/CesiumGltfModelResourceCache.h"
#include "Runtime/Private/Metadata/CesiumFeatureStyleEncoding.h"

#if defined(CESIUM_GD_MODULE)
#include "core/object/object_id.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/core/object.hpp"
using namespace godot;
#endif

#include <memory>

class Cesium3DTileset;
class CesiumLoadFailureQueue;

class GodotPrepareRenderResources final : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
	GodotPrepareRenderResources(
		Cesium3DTileset* source,
		const std::shared_ptr<CesiumTilesetRuntimeStatistics>& statistics,
		const std::shared_ptr<CesiumLoadFailureQueue>& failureQueue
	);
	void release_generation_resources();

	CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> prepareInLoadThread(
	  const CesiumAsync::AsyncSystem& asyncSystem,
	  Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
	  const glm::dmat4& transform,
	  const std::any& rendererOptions) override;

	void* prepareInMainThread(Cesium3DTilesSelection::Tile& tile, void* pLoadThreadResult) override;

	Cesium3DTilesSelection::MainThreadRendererResourcesPreparationResult
	prepareInMainThreadIncrementally(
		Cesium3DTilesSelection::Tile& tile,
		void* pLoadThreadResult,
		double timeBudgetMilliseconds
	) override;

	void free(
		Cesium3DTilesSelection::Tile& tile,
		void* pLoadThreadResult,
		void* pMainThreadResult) noexcept override;

	void attachRasterInMainThread(
		const Cesium3DTilesSelection::Tile& tile,
		int32_t overlayTextureCoordinateID,
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pMainThreadRendererResources,
		const glm::dvec2& translation,
		const glm::dvec2& scale) override;

	void detachRasterInMainThread(
		const Cesium3DTilesSelection::Tile& tile,
		int32_t overlayTextureCoordinateID,
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pMainThreadRendererResources) noexcept override;


	void* prepareRasterInLoadThread(
		CesiumImage::ImageAsset& image,
		const std::any& rendererOptions) override;

	/**
	 * @brief Further preprares a raster overlay tile.
	 *
	 * This is called after {@link prepareRasterInLoadThread}, and unlike that
	 * method, this one is called from the same thread that called
	 * {@link Tileset::updateView}.
	 *
	 * @param rasterTile The raster tile to prepare.
	 * @param pLoadThreadResult The value returned from
	 * {@link prepareRasterInLoadThread}.
	 * @returns Arbitrary data representing the result of the load process. Note
	 * that the value returned by {@link prepareRasterInLoadThread} will _not_ be
	 * automatically preserved and passed to {@link free}. If you need to free
	 * that value, do it in this method before returning. If you need that value
	 * later, add it to the object returned from this method.
	 */
	void* prepareRasterInMainThread(
		CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pLoadThreadResult) override;

	/**
	 * @brief Frees previously-prepared renderer resources for a raster tile.
	 *
	 * This method is always called from the thread that destroyed the
	 * {@link RasterOverlayTile}. When raster overlays are used with tilesets,
	 * this is the thread that called {@link Tileset::updateView} or deleted the
	 * tileset.
	 *
	 * @param rasterTile The tile for which to free renderer resources.
	 * @param pLoadThreadResult The result returned by
	 * {@link prepareRasterInLoadThread}. If {@link prepareRasterInMainThread}
	 * has already been called, this parameter will be `nullptr`.
	 * @param pMainThreadResult The result returned by
	 * {@link prepareRasterInMainThread}. If {@link prepareRasterInMainThread}
	 * has not yet been called, this parameter will be `nullptr`.
	 */
	void freeRaster(
		const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
		void* pLoadThreadResult,
		void* pMainThreadResult) noexcept override;

private:
	Cesium3DTileset* resolve_tileset() const;

	ObjectID m_tileset;
	bool m_createPhysicsMeshes = false;
	bool m_isGeoreferenced = false;
	bool m_isTrueOrigin = false;
	uint64_t m_maximumPrimitiveGeometryBytes = 0;
	uint64_t m_maximumPrimitiveTextureBytes = 0;
	bool m_enableLodTransitionDither = false;
	bool m_enableTranslucencyDepthPrepass = true;
	CesiumFeatureStyleEncodingDescription m_featureStyle;
	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_statistics;
	std::shared_ptr<CesiumLoadFailureQueue> m_failureQueue;
	std::shared_ptr<CesiumGltfImageAssetResourceCache> m_sharedImageCache;
	std::shared_ptr<CesiumGltfModelResourceCache> m_sharedModelCache;
};

#endif // !GODOT_PREPARE_RENDER_RESOURCES_H
