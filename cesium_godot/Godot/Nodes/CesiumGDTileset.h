// Godot adaptation reviewed against Cesium for Unreal v2.29.0:
// - Source/CesiumRuntime/Public/Cesium3DTileset.h
// - Source/CesiumRuntime/Private/Cesium3DTileset.cpp
// Last upstream review: Cesium for Unreal v2.29.0.

#ifndef CESIUM_GD_TILESET_H
#define CESIUM_GD_TILESET_H

#if defined(CESIUM_GD_MODULE)
#include "core/math/aabb.h"
#include "core/math/vector4.h"
#include "core/variant/array.h"
#include "core/variant/packed_byte_array.h"
#include "core/variant/packed_float64_array.h"
#include "core/variant/packed_vector3_array.h"
#include "core/string/node_path.h"
#include "scene/3d/node_3d.h"
#elif defined(CESIUM_GD_EXT)
#include "godot_cpp/core/property_info.hpp"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector4.hpp>
using namespace godot;
#endif

#include "Cesium3DTilesSelection/Tileset.h"
#include "glm/ext/vector_double3.hpp"

#include <chrono>
#include <mutex>
#include <string>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Models/CesiumDataSource.h"
#include "Runtime/Private/Georeference/CesiumCameraPredictor.h"
#include "Runtime/Private/Diagnostics/CesiumHardwareCapabilities.h"
#include "Runtime/Private/CesiumTilesetRuntimeStatistics.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"

namespace Cesium3DTilesSelection {
	class Tileset;
	class Tile;
	class TilesetExternals;
	class ITileExcluder;
}

namespace CesiumGltf {
	class Model;
}

namespace CesiumAsync {
	class AsyncSystem;
	class ICacheDatabase;
	class IAssetAccessor;
}

namespace CesiumGeospatial {
	class Cartographic;
}

class OpaqueTilesetOptions;


class CesiumRasterOverlay;

class CesiumGeoreference;

class CesiumGDCreditSystem;
class CesiumCameraManager;
class CesiumGodotOcclusionProxy;
class CesiumGodotOcclusionProxyPool;

class Cesium3DTile;

class Cesium3DTilesetLifecycleEventReceiver;
class CesiumRasterOverlayBinding;
class CesiumLoadedTilePrimitive;
class CesiumSampleHeightMostDetailedRequest;
class GodotPrepareRenderResources;
class NetworkAssetAccessor;
class CesiumLoadFailure;
class CesiumLoadFailureQueue;
class CesiumTileDebugState;
class CesiumMetadataStyle;
class CesiumPointCloudShading;
struct CesiumGDPreparedModel;

enum class EBoundingType {
	None,
	Sphere,
	Box,
	Region,
	RegionWithLooseFittingHeights,
	CellVolume,
	CylinderRegion
};

class Cesium3DTileset : public Node3D
{
	GDCLASS(Cesium3DTileset, Node3D)

public:
	Cesium3DTileset();
	~Cesium3DTileset();
#pragma region Public Editor Methods

	void set_maximum_screen_space_error(real_t error);

	real_t get_maximum_screen_space_error() const;
	
	void set_maximum_simultaneous_tile_loads(uint32_t count);

	uint32_t get_maximum_simultaneous_tile_loads() const;

	void set_stale_request_cancellation_enabled(bool enabled);

	bool get_stale_request_cancellation_enabled() const;

	void set_preload_ancestors(bool preload);

	bool get_preload_ancestors() const;

	void set_preload_siblings(bool preload);

	bool get_preload_siblings() const;

	void set_loading_descendant_limit(uint32_t limit);

	uint32_t get_loading_descendant_limit() const;

	void set_forbid_holes(bool forbidHoles);

	bool get_forbid_holes() const;

	void set_frustum_culling_enabled(bool enabled);

	bool get_frustum_culling_enabled() const;

	void set_occlusion_culling_enabled(bool enabled);

	bool get_occlusion_culling_enabled() const;

	void set_occlusion_pool_size(int32_t size);

	int32_t get_occlusion_pool_size() const;

	void set_delay_refinement_for_occlusion(bool enabled);

	bool get_delay_refinement_for_occlusion() const;

	void set_fog_culling_enabled(bool enabled);

	bool get_fog_culling_enabled() const;

	void set_enforce_culled_screen_space_error(bool enforce);

	bool get_enforce_culled_screen_space_error() const;

	void set_culled_screen_space_error(real_t error);

	real_t get_culled_screen_space_error() const;

	void set_render_tiles_under_camera(bool enabled);

	bool get_render_tiles_under_camera() const;

	void set_lod_transitions_enabled(bool enabled);

	bool get_lod_transitions_enabled() const;

	void set_lod_transition_length(real_t seconds);

	real_t get_lod_transition_length() const;

	void set_kick_descendants_while_fading_in(bool enabled);

	bool get_kick_descendants_while_fading_in() const;

	void set_translucency_sort_priority(int32_t priority);

	int32_t get_translucency_sort_priority() const;

	void set_translucency_depth_prepass_enabled(bool enabled);

	bool get_translucency_depth_prepass_enabled() const;

	void set_maximum_cached_bytes(int64_t bytes);

	int64_t get_maximum_cached_bytes() const;

	void set_automatic_hardware_budgets_enabled(bool enabled);

	bool get_automatic_hardware_budgets_enabled() const;

	void set_hardware_budget_profile(int32_t profile);

	int32_t get_hardware_budget_profile() const;

	void set_automatic_cache_budget_share(real_t share);

	real_t get_automatic_cache_budget_share() const;

	int64_t recalculate_automatic_hardware_budgets();

	Dictionary get_hardware_capabilities();

	int64_t get_recommended_total_cache_bytes(int32_t profile);

	void set_worker_thread_count(uint32_t count);

	uint32_t get_worker_thread_count() const;

	void reset_worker_thread_count_to_automatic();

	void set_main_thread_loading_time_limit_ms(real_t milliseconds);

	real_t get_main_thread_loading_time_limit_ms() const;

	void set_maximum_primitive_geometry_upload_bytes(int64_t bytes);

	int64_t get_maximum_primitive_geometry_upload_bytes() const;

	void set_maximum_primitive_texture_upload_bytes(int64_t bytes);

	int64_t get_maximum_primitive_texture_upload_bytes() const;

	void set_tile_cache_unload_time_limit_ms(real_t milliseconds);

	real_t get_tile_cache_unload_time_limit_ms() const;

	void set_http_cache_enabled(bool enabled);

	bool get_http_cache_enabled() const;

	void set_http_cache_path(const String& path);

	const String& get_http_cache_path() const;

	String get_resolved_http_cache_path() const;

	void set_http_cache_maximum_items(int64_t maximumItems);

	int64_t get_http_cache_maximum_items() const;

	void set_http_cache_maximum_data_bytes(int64_t maximumBytes);

	int64_t get_http_cache_maximum_data_bytes() const;

	void set_http_cache_prune_interval_requests(int32_t requests);

	int32_t get_http_cache_prune_interval_requests() const;

	bool clear_http_cache();

	bool prune_http_cache();

	void set_maximum_network_retries(int32_t retries);

	int32_t get_maximum_network_retries() const;

	void set_network_retry_initial_delay_seconds(real_t seconds);

	real_t get_network_retry_initial_delay_seconds() const;

	void set_network_retry_maximum_delay_seconds(real_t seconds);

	real_t get_network_retry_maximum_delay_seconds() const;

	void set_movement_prediction_enabled(bool enabled);

	bool get_movement_prediction_enabled() const;

	void set_movement_prediction_seconds(real_t seconds);

	real_t get_movement_prediction_seconds() const;

	void set_movement_prediction_minimum_speed(real_t metersPerSecond);

	real_t get_movement_prediction_minimum_speed() const;

	void set_movement_prediction_maximum_distance(real_t meters);

	real_t get_movement_prediction_maximum_distance() const;

	void set_movement_prediction_weight(real_t weight);

	real_t get_movement_prediction_weight() const;

	void set_turn_prediction_enabled(bool enabled);

	bool get_turn_prediction_enabled() const;

	void set_turn_prediction_minimum_angular_speed_degrees(real_t degreesPerSecond);

	real_t get_turn_prediction_minimum_angular_speed_degrees() const;

	void set_turn_prediction_maximum_angle_degrees(real_t degrees);

	real_t get_turn_prediction_maximum_angle_degrees() const;

	void set_zoom_out_prediction_enabled(bool enabled);

	bool get_zoom_out_prediction_enabled() const;

	void set_zoom_out_prediction_minimum_rate(real_t rate);

	real_t get_zoom_out_prediction_minimum_rate() const;

	void set_zoom_out_prediction_maximum_scale(real_t scale);

	real_t get_zoom_out_prediction_maximum_scale() const;

	void set_url(const String& url);

	const String& get_url() const;

	void set_request_headers(const Dictionary& headers);

	Dictionary get_request_headers() const;

	void set_credit(const String& credit);

	String get_credit() const;

	void set_credit_system(CesiumGDCreditSystem* creditSystem);

	CesiumGDCreditSystem* get_credit_system() const;

	CesiumGDCreditSystem* resolve_credit_system();

	CesiumGDCreditSystem* get_resolved_credit_system() const;

	void invalidate_resolved_credit_system();

	void set_show_credits_on_screen(bool showOnScreen);

	bool get_show_credits_on_screen() const;

	void set_generate_missing_normals_smooth(bool shouldGenerate);

	bool get_generate_missing_normals_smooth() const;

	int get_data_source() const;

	void set_data_source(int data_source);

	void set_ion_asset_id(int64_t id);

	int64_t get_ion_asset_id() const;
	
	void set_create_physics_meshes(bool shouldCreate);

	bool get_create_physics_meshes() const;

	void set_camera_manager_path(const NodePath& cameraManagerPath);

	NodePath get_camera_manager_path() const;

	bool get_show_hierarchy() const;

	void set_show_hierarchy(bool show);

#pragma endregion

	void update_tileset(const Transform3D& cameraTransform);

	void set_debug_boundig_volumes_func(const Callable& onTileDrawn);
	
	bool is_initial_loading_finished() const;
	bool has_active_tileset() const;
	Dictionary get_streaming_statistics() const;
	void set_debug_tile_state_capture_enabled(bool enabled);
	bool get_debug_tile_state_capture_enabled() const;
	void set_debug_tile_state_limit(int32_t limit);
	int32_t get_debug_tile_state_limit() const;
	Array get_debug_tile_states() const;
	int64_t get_debug_tile_state_frame_number() const;
	bool get_debug_tile_states_truncated() const;
	Ref<CesiumBoundingVolume> get_tileset_bounds() const;
	AABB get_tileset_source_aabb() const;
	Ref<CesiumSampleHeightMostDetailedRequest> sample_height_most_detailed(
		const PackedVector3Array& longitudeLatitudeHeight
	);
	Ref<CesiumSampleHeightMostDetailedRequest>
	sample_height_most_detailed_exact(
		const PackedFloat64Array& longitudeLatitudeHeightComponents
	);

	void add_overlay(CesiumRasterOverlay* overlay);
	void remove_overlay(CesiumRasterOverlay* overlay);
	CesiumGeospatial::Ellipsoid get_raster_overlay_ellipsoid() const;
	void add_tile_excluder(
		const std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>& excluder
	);
	void remove_tile_excluder(
		const std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>& excluder
	);

	void free_tile(Cesium3DTile* tileInstance, uint64_t tileHash);
	
	bool is_georeferenced(CesiumGeoreference** outRef) const;

	void move_origin(const glm::dvec3& enginePos);
	void apply_georeference();

	void set_for_each_tile_func(const Callable& onTileFunc);

	void set_lifecycle_event_receiver(Cesium3DTilesetLifecycleEventReceiver* receiver);

	Cesium3DTilesetLifecycleEventReceiver* get_lifecycle_event_receiver() const;

	void set_metadata_style(const Ref<CesiumMetadataStyle>& style);
	Ref<CesiumMetadataStyle> get_metadata_style() const;
	void refresh_metadata_style();
	void set_point_cloud_shading(const Ref<CesiumPointCloudShading>& shading);
	Ref<CesiumPointCloudShading> get_point_cloud_shading() const;
	void refresh_point_cloud_shading();
	void refresh_translucency_sort_priority();

	// Renderer integration methods. Lifecycle callbacks are main-thread only.
	void finalize_loaded_tile(
		Cesium3DTile* tile,
		const CesiumGltf::Model& model,
		const CesiumGDPreparedModel& prepared,
		double tileGeometricError,
		bool usesAdditiveRefinement
	);

	void notify_tile_visibility_changed(Cesium3DTile* tile, bool visible);

	void notify_tile_unloading(Cesium3DTile* tile);
	void notify_raster_overlay_attached(
		const Ref<CesiumRasterOverlayBinding>& binding
	);
	void notify_raster_overlay_detaching(
		const Ref<CesiumRasterOverlayBinding>& binding
	);
	Ref<CesiumRasterOverlayBinding> attach_raster_overlay(
		const Ref<CesiumLoadedTilePrimitive>& primitive,
		const String& overlayKey,
		const Ref<Texture2D>& texture,
		int32_t textureCoordinateId,
		int32_t textureCoordinateIndex,
		const Vector2& translation,
		const Vector2& scale
	);
	bool detach_raster_overlay(
		const Ref<CesiumLoadedTilePrimitive>& primitive,
		const String& overlayKey,
		const Ref<Texture2D>& expectedTexture = Ref<Texture2D>()
	);

	std::shared_ptr<CesiumLoadFailureQueue> get_load_failure_queue() const;

	// Internal provider bridge. These are the exact scheduler and decorated
	// asset accessor owned by the active Native tileset, so auxiliary content
	// providers share its worker pool, HTTP cache, retry policy, and teardown.
	const CesiumAsync::AsyncSystem* get_native_async_system() const;
	std::shared_ptr<CesiumAsync::IAssetAccessor>
	get_native_asset_accessor() const;

	CesiumGeoreference* get_georeference_node() const;
	
	void _enter_tree() override;
	void _exit_tree() override;

	void _ready() override;

private:

	void recreate_tileset();
	void release_active_tileset();

	void load_tileset();

	void reset_movement_prediction();
	void capture_hardware_capabilities();
	void apply_automatic_cache_budget();
	Ref<CesiumSampleHeightMostDetailedRequest> start_height_query(
		std::vector<CesiumGeospatial::Cartographic>&& positions
	);
	void forget_height_request(const ObjectID& requestId);
	void cancel_height_requests(const String& warning);
	void _on_metadata_style_changed();
	void disconnect_metadata_style();
	void _on_point_cloud_shading_changed();
	void disconnect_point_cloud_shading();
	void schedule_load_failure_dispatches();
	void emit_load_failure_deferred(const Ref<CesiumLoadFailure>& failure);
	void capture_debug_tile_states(
		const Cesium3DTilesSelection::ViewUpdateResult& updateResult
	);
	Transform3D get_debug_world_bounds_transform(
		const Ref<CesiumBoundingVolume>& bounds
	) const;
	AABB get_occlusion_world_bounds(
		const Cesium3DTilesSelection::Tile& tile
	) const;
	void consume_occlusion_results();
	void submit_occlusion_queries(Viewport* viewport);
	void execute_occlusion_query(
		const RID& viewportRid,
		const Array& bounds,
		int64_t generation
	);
	void reset_occlusion_bridge();

	Cesium3DTilesSelection::TilesetExternals create_tileset_externals();

	void render_tile_as_node(const Cesium3DTilesSelection::Tile& tile);

	void despawn_tile(const Cesium3DTilesSelection::Tile& tile);

	void despawn_tile_deferred(const Cesium3DTilesSelection::Tile& tile);

	bool try_get_tile_from_instance_id(const ObjectID& objectId, Cesium3DTile** outNode);

	void register_tile(Cesium3DTile *instance, size_t hash);

	uint32_t update_property_usage_flags(const PropertyInfo& property) const;
	
	std::unique_ptr<Cesium3DTilesSelection::Tileset> m_activeTileset = nullptr;
	std::shared_ptr<GodotPrepareRenderResources> m_renderResourcesProvider;
	mutable Ref<CesiumBoundingVolume> m_tilesetBounds;
	std::vector<Ref<CesiumSampleHeightMostDetailedRequest>>
		m_activeHeightRequests;
	std::unique_ptr<Cesium3DTilesSelection::TilesetViewGroup>
		m_predictionViewGroup;
	std::shared_ptr<CesiumGodotOcclusionProxyPool> m_occlusionProxyPool;
	struct OcclusionSubmission {
		uint64_t generation = 0;
		std::vector<const Cesium3DTilesSelection::Tile*> tiles;
	};
	OcclusionSubmission m_occlusionSubmission;
	mutable std::mutex m_occlusionResultMutex;
	PackedByteArray m_completedOcclusionResults;
	uint64_t m_completedOcclusionGeneration = 0;
	uint64_t m_occlusionGeneration = 0;
	bool m_occlusionQueryInFlight = false;
	bool m_occlusionBridgeAvailable = false;
	int32_t m_lastOcclusionVisibleResultCount = 0;
	int32_t m_lastOcclusionOccludedResultCount = 0;
	int32_t m_lastOcclusionUnavailableResultCount = 0;
	bool m_occlusionCullingRequested = false;
	bool m_delayRefinementForOcclusion = false;
	int32_t m_occlusionPoolSize = 1000;


	OpaqueTilesetOptions* m_tilesetConfig;

	bool m_createPhysicsMeshes = true;

	String m_url{};
	Dictionary m_requestHeaders;
	NodePath m_cameraManagerPath;

	int64_t m_cesiumIonAssetId = 0;

	bool m_initialLoadingFinished;

	bool m_showHierarchy;

	CesiumDataSource m_selectedDataSource = CesiumDataSource::FromCesiumIon;

	CesiumGeoreference* m_georeference = nullptr;

	ObjectID m_configuredCreditSystem;
	ObjectID m_resolvedCreditSystem;

	Callable m_debugVolumesFunction;

	Callable m_forEachTileFunction;

	ObjectID m_lifecycleEventReceiver;
	Ref<CesiumMetadataStyle> m_metadataStyle;
	uint64_t m_metadataStyleEncodingRevision = 0;
	Ref<CesiumPointCloudShading> m_pointCloudShading;
	int32_t m_translucencySortPriority = 0;
	bool m_translucencyDepthPrepassEnabled = true;

	std::unordered_set<std::string> m_reportedFailedTiles;
	uint64_t m_terminalTileFailureCount = 0;

	uint32_t m_workerThreadCount = 1;
	bool m_workerThreadCountAutomatic = true;
	bool m_automaticHardwareBudgetsEnabled = false;
	CesiumHardwareBudgetProfile m_hardwareBudgetProfile =
		CesiumHardwareBudgetProfile::Balanced;
	real_t m_automaticCacheBudgetShare = 1.0;
	CesiumHardwareCapabilities m_hardwareCapabilities;
	bool m_hardwareCapabilitiesCaptured = false;
	String m_cacheBudgetSource = "cesium_native_default";
	int64_t m_maximumPrimitiveGeometryUploadBytes = 16 * 1024 * 1024;
	int64_t m_maximumPrimitiveTextureUploadBytes = 16 * 1024 * 1024;

	std::shared_ptr<CesiumTilesetRuntimeStatistics> m_runtimeStatistics;

	bool m_httpCacheEnabled = true;
	String m_httpCachePath =
		"user://cache/cesium-request-cache.sqlite";
	uint64_t m_httpCacheMaximumItems = 20480;
	uint64_t m_httpCacheMaximumDataBytes = 1024ULL * 1024 * 1024;
	int32_t m_httpCachePruneIntervalRequests = 10000;
	String m_resolvedHttpCachePath;
	std::shared_ptr<CesiumAsync::ICacheDatabase> m_requestCacheDatabase;
	std::shared_ptr<NetworkAssetAccessor> m_networkAssetAccessor;
	std::shared_ptr<CesiumLoadFailureQueue> m_loadFailureQueue;
	int32_t m_maximumNetworkRetries = 3;
	real_t m_networkRetryInitialDelaySeconds = 0.25;
	real_t m_networkRetryMaximumDelaySeconds = 4.0;

	bool m_movementPredictionEnabled = true;
	real_t m_movementPredictionSeconds = 1.0;
	real_t m_movementPredictionMinimumSpeed = 1.0;
	real_t m_movementPredictionMaximumDistance = 1000.0;
	real_t m_movementPredictionWeight = 0.25;
	bool m_turnPredictionEnabled = true;
	real_t m_turnPredictionMinimumAngularSpeedDegrees = 5.0;
	real_t m_turnPredictionMaximumAngleDegrees = 90.0;
	bool m_zoomOutPredictionEnabled = true;
	real_t m_zoomOutPredictionMinimumRate = 0.1;
	real_t m_zoomOutPredictionMaximumScale = 2.0;
	CesiumCameraPredictor m_cameraPredictor;
	ObjectID m_predictionCameraId;
	bool m_lastPredictionActive = false;
	bool m_lastTranslationPredictionActive = false;
	bool m_lastTurnPredictionActive = false;
	bool m_lastZoomOutPredictionActive = false;
	double m_lastPredictionSpeed = 0.0;
	double m_lastPredictionDistance = 0.0;
	double m_lastPredictionAngularSpeedDegrees = 0.0;
	double m_lastPredictionAngleDegrees = 0.0;
	double m_lastPredictionZoomOutRate = 0.0;
	double m_lastPredictionProjectionScale = 1.0;
	glm::dvec3 m_lastPredictedViewDirection{0.0, 0.0, -1.0};
	int32_t m_lastPredictionWorkerQueueLength = 0;
	int32_t m_lastPredictionMainQueueLength = 0;
	bool m_lastPredictionSuppressedByLodTransitions = false;

	int32_t m_lastWorkerQueueLength = 0;
	int32_t m_lastMainQueueLength = 0;
	int32_t m_lastSelectedTileCount = 0;
	int32_t m_lastFadingTileCount = 0;
	uint32_t m_lastTilesVisited = 0;
	uint32_t m_lastCulledTilesVisited = 0;
	uint32_t m_lastTilesCulled = 0;
	uint32_t m_lastTilesOccluded = 0;
	uint32_t m_lastTilesWaitingForOcclusionResults = 0;
	uint32_t m_lastTilesKicked = 0;
	uint32_t m_lastMaximumDepthVisited = 0;
	bool m_lastViewStateValid = false;
	int32_t m_lastViewProjectionType = 0;
	String m_lastViewProjectionTypeName = "unavailable";
	bool m_lastViewKeepWidth = false;
	Vector2 m_lastViewViewportSize;
	glm::dvec3 m_lastViewPosition{0.0};
	glm::dvec3 m_lastViewDirection{0.0};
	glm::dvec3 m_lastViewUp{0.0};
	double m_lastViewHorizontalFieldOfViewRadians = 0.0;
	double m_lastViewVerticalFieldOfViewRadians = 0.0;
	Vector4 m_lastViewPlaneExtents;
	double m_lastViewNearPlane = 0.0;
	int32_t m_lastSelectionViewCount = 0;
	int32_t m_lastSelectionRenderViewCount = 0;
	int32_t m_lastSelectionInvalidCameraCount = 0;
	int32_t m_lastSelectionWrongWorldCameraCount = 0;
	int32_t m_lastSelectionDuplicateCameraCount = 0;
	bool m_lastSelectionCameraManagerConfigured = false;
	bool m_lastSelectionCameraManagerResolved = false;
	int32_t m_lastLodTransitionActiveTileCount = 0;
	int32_t m_lastLodTransitionSupportedPrimitiveCount = 0;
	int32_t m_lastLodTransitionUnsupportedPrimitiveCount = 0;
	int32_t m_lastLodTransitionCompatibleRenderNodeCount = 0;
	double m_lastLodTransitionMinimumPercentage = 1.0;
	double m_lastLodTransitionMaximumPercentage = 1.0;
	uint64_t m_selectionMicroseconds = 0;
	uint64_t m_loadTilesMicroseconds = 0;
	bool m_debugTileStateCaptureEnabled = false;
	int32_t m_debugTileStateLimit = 512;
	Array m_debugTileStates;
	uint64_t m_debugTileStateFrameNumber = 0;
	bool m_debugTileStatesTruncated = false;
protected:
	static void _bind_methods();

	void _get_property_list(List<PropertyInfo>* properties) const;

	bool _set(const StringName& p_name, const Variant& p_property);
	bool _get(const StringName& p_name, Variant& r_property) const;
};

#endif
