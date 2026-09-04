#include "register_types.h"


#include "Models/Cesium3DTile.h"
#include "Runtime/Public/Cesium3DTilesetLifecycleEventReceiver.h"
#include "Runtime/Public/CesiumCameraManager.h"
#include "Runtime/Public/CesiumLoadedTilePrimitive.h"
#include "Runtime/Public/CesiumRasterOverlayBinding.h"
#include "Runtime/Public/Renderer/CesiumGltfInstancedComponent.h"
#include "Runtime/Public/Renderer/CesiumPointCloudShading.h"
#include "Runtime/Public/Bounds/CesiumBoundingVolume.h"
#include "Runtime/Public/Georeference/CesiumSampleHeightResult.h"
#include "Runtime/Public/Georeference/CesiumSampleHeightMostDetailedRequest.h"
#include "Runtime/Public/Georeference/CesiumEllipsoid.h"
#include "Runtime/Public/Georeference/CesiumGlobeAnchor.h"
#include "Runtime/Public/Georeference/CesiumOriginShift.h"
#include "Runtime/Public/Georeference/CesiumFlyTo.h"
#include "Runtime/Public/Georeference/CesiumCartographicPolygon.h"
#include "Runtime/Public/Geocoder/CesiumGeocoderService.h"
#include "Runtime/Public/Diagnostics/CesiumLoadFailure.h"
#include "Runtime/Public/Diagnostics/CesiumTileDebugState.h"
#include "Runtime/Public/TileSelection/CesiumTileExclusionContext.h"
#include "Runtime/Public/Metadata/CesiumMetadataProperty.h"
#include "Runtime/Public/Metadata/CesiumPropertyTable.h"
#include "Runtime/Public/Metadata/CesiumModelMetadata.h"
#include "Runtime/Public/Metadata/CesiumMetadataEnum.h"
#include "Runtime/Public/Metadata/CesiumFeatureIdSet.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveFeatures.h"
#include "Runtime/Public/Metadata/CesiumMetadataPicking.h"
#include "Runtime/Public/Metadata/CesiumMetadataStyle.h"
#include "Runtime/Public/Metadata/CesiumPropertyAttributeProperty.h"
#include "Runtime/Public/Metadata/CesiumPropertyAttribute.h"
#include "Runtime/Public/Metadata/CesiumPropertyTextureProperty.h"
#include "Runtime/Public/Metadata/CesiumPropertyTexture.h"
#include "Runtime/Public/Metadata/CesiumPrimitiveMetadata.h"
#include "Runtime/Private/Renderer/CesiumGltfImageAssetResourceCache.h"
#include "Godot/Nodes/CesiumGDCreditSystem.h"
#include "Runtime/Public/Credits/CesiumCredit.h"
#include "Runtime/Public/Networking/CesiumUrlUtility.h"
#include "Runtime/Public/Ion/CesiumIonEditorSession.h"
#include "Godot/Nodes/CesiumGDTileset.h"
#include "Godot/Nodes/CesiumTileDebugVisualizer.h"
#include "Godot/Nodes/CesiumTileExcluder.h"
#include "Models/CesiumHTTPRequestNode.h"
#include "Godot/Nodes/CesiumGeoreferencedMesh.h"
#include "Utils/CesiumDebugUtils.h"
#include "Godot/Nodes/CesiumGeoreference.h"
#include "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumDebugColorizeTilesRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumBingMapsRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumGoogleMapTilesRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumIonRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumTileMapServiceRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumUrlTemplateRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumWebMapServiceRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumWebMapTileServiceRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumPolygonRasterOverlay.h"
#include "Runtime/Public/RasterOverlays/CesiumGeoJsonDocumentRasterOverlay.h"
#include "Runtime/Public/Vector/CesiumGeoJsonDocument.h"
#include "Runtime/Public/Vector/CesiumGeoJsonObject.h"
#include "Runtime/Public/Vector/CesiumVectorStyle.h"
#include "Models/CesiumGDPanel.h"
#include "Models/CesiumGDConfig.h"
#include "Utils/CesiumGDAssetBuilder.h"
#include "Utils/TokenTroubleShooting.h"		
#include "godot_cpp/classes/engine.hpp"
#include <cstdio>

#if defined(CESIUM_GD_EXT)
#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>
using namespace godot;
#elif defined(CESIUM_GD_MODULE)
#include <core/object/class_db.h>
#endif


void initialize_cesium_godot_module(ModuleInitializationLevel p_level) {
	//We will probably have to switch the module initialization level to the editor
	//But we will keep it to the Scene for some testing that the library has been properly linked
	if (p_level != ModuleInitializationLevel::MODULE_INITIALIZATION_LEVEL_SCENE)
		return;
	//We will have to register all external classes to the class DB (we probably don't want this for common DataStructures, but rather Nodes)
	ClassDB::register_class<CesiumGeoreference>();
	ClassDB::register_class<CesiumEllipsoid>();
	ClassDB::register_class<CesiumGlobeAnchor>();
	ClassDB::register_class<CesiumOriginShift>();
	ClassDB::register_class<CesiumFlyTo>();
	ClassDB::register_class<CesiumCartographicPolygon>();
	ClassDB::register_class<CesiumGeocoderAttribution>();
	ClassDB::register_class<CesiumGeocoderFeature>();
	ClassDB::register_class<CesiumGeocoderResult>();
	ClassDB::register_class<CesiumGeocoderRequest>();
	ClassDB::register_class<CesiumGeocoderService>();
	ClassDB::register_class<CesiumCameraManager>();
	ClassDB::register_class<Cesium3DTileset>();
	ClassDB::register_class<CesiumTileDebugVisualizer>();
	ClassDB::register_class<CesiumTileExcluder>();
	ClassDB::register_class<CesiumHTTPRequestNode>();
	ClassDB::register_class<CesiumDebugUtils>();
	ClassDB::register_class<CesiumGDPanel>();
	ClassDB::register_abstract_class<CesiumRasterOverlay>();
	ClassDB::register_class<CesiumDebugColorizeTilesRasterOverlay>();
	ClassDB::register_class<CesiumBingMapsRasterOverlay>();
	ClassDB::register_class<CesiumGoogleMapTilesRasterOverlay>();
	ClassDB::register_class<CesiumIonRasterOverlay>();
	ClassDB::register_class<CesiumTileMapServiceRasterOverlay>();
	ClassDB::register_class<CesiumUrlTemplateRasterOverlay>();
	ClassDB::register_class<CesiumWebMapServiceRasterOverlay>();
	ClassDB::register_class<CesiumWebMapTileServiceRasterOverlay>();
	ClassDB::register_class<CesiumPolygonRasterOverlay>();
	ClassDB::register_class<CesiumGeoJsonDocument>();
	ClassDB::register_class<CesiumGeoJsonObject>();
	ClassDB::register_class<CesiumVectorStyle>();
	ClassDB::register_class<CesiumGeoJsonDocumentRasterOverlay>();
	ClassDB::register_class<CesiumGDConfig>();
	ClassDB::register_class<CesiumGDAssetBuilder>();
	ClassDB::register_class<TokenTroubleshooting>();
	ClassDB::register_class<GeoreferencedMesh>();
	ClassDB::register_class<Cesium3DTile>();
	ClassDB::register_class<CesiumLoadedTilePrimitive>();
	ClassDB::register_class<CesiumRasterOverlayBinding>();
	ClassDB::register_class<CesiumGltfInstancedComponent>();
	ClassDB::register_class<CesiumPointCloudShading>();
	ClassDB::register_class<CesiumBoundingVolume>();
	ClassDB::register_class<CesiumSampleHeightResult>();
	ClassDB::register_class<CesiumSampleHeightMostDetailedRequest>();
	ClassDB::register_class<CesiumLoadFailure>();
	ClassDB::register_class<CesiumTileDebugState>();
	ClassDB::register_class<CesiumTileExclusionContext>();
	ClassDB::register_class<Cesium3DTilesetLifecycleEventReceiver>();
	ClassDB::register_class<CesiumMetadataProperty>();
	ClassDB::register_class<CesiumPropertyTable>();
	ClassDB::register_class<CesiumModelMetadata>();
	ClassDB::register_class<CesiumMetadataEnum>();
	ClassDB::register_class<CesiumFeatureIdSet>();
	ClassDB::register_class<CesiumPrimitiveFeatures>();
	ClassDB::register_class<CesiumMetadataPicking>();
	ClassDB::register_class<CesiumMetadataStyle>();
	ClassDB::register_class<CesiumPropertyAttributeProperty>();
	ClassDB::register_class<CesiumPropertyAttribute>();
	ClassDB::register_class<CesiumPropertyTextureProperty>();
	ClassDB::register_class<CesiumPropertyTexture>();
	ClassDB::register_class<CesiumPrimitiveMetadata>();
	ClassDB::register_class<CesiumCredit>();
	ClassDB::register_class<CesiumGDCreditSystem>();
	ClassDB::register_class<CesiumUrlUtility>();
	ClassDB::register_class<CesiumIonEditorSession>();
}

void uninitialize_cesium_godot_module(ModuleInitializationLevel p_level) {
	if (p_level != ModuleInitializationLevel::MODULE_INITIALIZATION_LEVEL_SCENE)
		return;
	CesiumGltfImageAssetResourceCache::release_all_renderer_resources();
}

extern "C" {
	GDExtensionBool GDE_EXPORT cesium_godot_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization* r_initialization){
		godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_cesium_godot_module);
		init_obj.register_terminator(uninitialize_cesium_godot_module);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
		return init_obj.init();
  }
}
