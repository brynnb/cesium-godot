#!/usr/bin/env python3
"""Guard the incremental Cesium-for-Unreal-aligned source layout."""

from __future__ import annotations

import re
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]
SOURCE_ROOT = REPOSITORY / "cesium_godot"

MIGRATED_FILES = (
    "Godot/Nodes/CesiumGDTileset.cpp",
    "Godot/Nodes/CesiumGDTileset.h",
	"Runtime/Private/CesiumCameraManager.cpp",
	"Runtime/Public/CesiumCameraManager.h",
    "Runtime/Private/RasterOverlays/CesiumRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumRasterOverlay.h",
    "Runtime/Private/RasterOverlays/CesiumRasterOverlayRendererOptions.h",
	"Runtime/Private/RasterOverlays/CesiumDebugColorizeTilesRasterOverlay.cpp",
	"Runtime/Public/RasterOverlays/CesiumDebugColorizeTilesRasterOverlay.h",
	"Runtime/Private/RasterOverlays/CesiumBingMapsRasterOverlay.cpp",
	"Runtime/Public/RasterOverlays/CesiumBingMapsRasterOverlay.h",
	"Runtime/Private/RasterOverlays/CesiumGoogleMapTilesRasterOverlay.cpp",
	"Runtime/Public/RasterOverlays/CesiumGoogleMapTilesRasterOverlay.h",
    "Runtime/Private/RasterOverlays/CesiumIonRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumIonRasterOverlay.h",
    "Runtime/Private/RasterOverlays/CesiumTileMapServiceRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumTileMapServiceRasterOverlay.h",
    "Runtime/Private/RasterOverlays/CesiumUrlTemplateRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumUrlTemplateRasterOverlay.h",
    "Runtime/Public/RasterOverlays/CesiumRasterOverlayProjection.h",
    "Runtime/Private/RasterOverlays/CesiumWebMapServiceRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumWebMapServiceRasterOverlay.h",
    "Runtime/Private/RasterOverlays/CesiumWebMapTileServiceRasterOverlay.cpp",
    "Runtime/Public/RasterOverlays/CesiumWebMapTileServiceRasterOverlay.h",
    "Godot/Nodes/CesiumTileDebugVisualizer.cpp",
    "Godot/Nodes/CesiumTileDebugVisualizer.h",
    "Godot/Nodes/CesiumGDCreditSystem.cpp",
    "Godot/Nodes/CesiumGDCreditSystem.h",
    "Runtime/Private/Async/GodotTaskProcessor.cpp",
    "Runtime/Private/Async/GodotTaskProcessor.h",
    "Runtime/Private/Networking/NetworkAssetAccessor.cpp",
    "Runtime/Private/Networking/NetworkAssetAccessor.h",
    "Runtime/Private/Networking/CesiumUrlUtility.cpp",
    "Runtime/Public/Networking/CesiumUrlUtility.h",
    "Runtime/Private/Networking/InstrumentedCacheDatabase.cpp",
    "Runtime/Private/Networking/InstrumentedCacheDatabase.h",
    "Runtime/Private/Renderer/CesiumGDModelLoader.cpp",
    "Runtime/Private/Renderer/CesiumGDModelLoader.h",
    "Runtime/Private/Renderer/CesiumGDTextureLoader.cpp",
    "Runtime/Private/Renderer/CesiumGDTextureLoader.h",
    "Runtime/Private/Renderer/GodotPrepareRenderResources.cpp",
    "Runtime/Private/Renderer/GodotPrepareRenderResources.h",
    "Runtime/Private/Renderer/CesiumPointCloudShading.cpp",
    "Runtime/Public/Renderer/CesiumPointCloudShading.h",
    "Runtime/Private/Diagnostics/CesiumTileDebugState.cpp",
    "Runtime/Public/Diagnostics/CesiumTileDebugState.h",
    "Runtime/Private/Diagnostics/CesiumHardwareCapabilities.cpp",
    "Runtime/Private/Diagnostics/CesiumHardwareCapabilities.h",
    "Runtime/Private/Credits/CesiumCredit.cpp",
    "Runtime/Public/Credits/CesiumCredit.h",
    "Runtime/Private/Georeference/CesiumGodotCameraProjection.cpp",
    "Runtime/Private/Georeference/CesiumGodotCameraProjection.h",
    "Runtime/Private/Geocoder/CesiumGeocoderService.cpp",
    "Runtime/Public/Geocoder/CesiumGeocoderService.h",
    "Runtime/Private/Georeference/CesiumCameraPredictor.cpp",
    "Runtime/Private/Georeference/CesiumCameraPredictor.h",
    "Godot/Nodes/CesiumGeoreference.cpp",
    "Godot/Nodes/CesiumGeoreference.h",
    "Godot/Nodes/CesiumGeoreferencedMesh.cpp",
    "Godot/Nodes/CesiumGeoreferencedMesh.h",
    "Runtime/Private/Georeference/CesiumEllipsoid.cpp",
    "Runtime/Public/Georeference/CesiumEllipsoid.h",
    "Runtime/Private/Georeference/CesiumGlobeAnchor.cpp",
    "Runtime/Public/Georeference/CesiumGlobeAnchor.h",
    "Runtime/Private/Georeference/CesiumOriginShift.cpp",
    "Runtime/Public/Georeference/CesiumOriginShift.h",
	"Runtime/Private/Georeference/CesiumFlyTo.cpp",
	"Runtime/Public/Georeference/CesiumFlyTo.h",
	"Runtime/Private/Georeference/CesiumCartographicPolygon.cpp",
	"Runtime/Public/Georeference/CesiumCartographicPolygon.h",
	"Godot/Nodes/CesiumTileExcluder.cpp",
	"Godot/Nodes/CesiumTileExcluder.h",
	"Runtime/Private/TileSelection/CesiumTileExclusionContext.cpp",
	"Runtime/Public/TileSelection/CesiumTileExclusionContext.h",
	"Runtime/Private/TileSelection/CesiumTileExcluderAdapter.cpp",
	"Runtime/Private/TileSelection/CesiumTileExcluderAdapter.h",
	"Runtime/Private/RasterOverlays/CesiumPolygonRasterOverlay.cpp",
	"Runtime/Public/RasterOverlays/CesiumPolygonRasterOverlay.h",
	"Runtime/Private/RasterOverlays/CesiumGeoJsonDocumentRasterOverlay.cpp",
	"Runtime/Public/RasterOverlays/CesiumGeoJsonDocumentRasterOverlay.h",
	"Runtime/Private/Vector/CesiumGeoJsonDocument.cpp",
	"Runtime/Public/Vector/CesiumGeoJsonDocument.h",
	"Runtime/Private/Vector/CesiumGeoJsonObject.cpp",
	"Runtime/Public/Vector/CesiumGeoJsonObject.h",
	"Runtime/Private/Vector/CesiumVectorStyle.cpp",
	"Runtime/Public/Vector/CesiumVectorStyle.h",
    "Runtime/Private/Materials/CesiumLodTransitionController.cpp",
    "Runtime/Private/Materials/CesiumLodTransitionController.h",
    "Runtime/Private/Metadata/CesiumFeatureStyleEncoding.h",
    "Runtime/Private/Metadata/CesiumMetadataStyle.cpp",
    "Runtime/Public/Metadata/CesiumMetadataStyle.h",
)

LEGACY_FILES = (
    "Models/CesiumGDTileset.cpp",
    "Models/CesiumGDTileset.h",
    "Models/CesiumGDRasterOverlay.cpp",
    "Models/CesiumGDRasterOverlay.h",
    "Godot/Nodes/CesiumGDRasterOverlay.cpp",
    "Godot/Nodes/CesiumGDRasterOverlay.h",
    "Implementations/GodotTaskProcessor.cpp",
    "Implementations/GodotTaskProcessor.h",
    "Implementations/NetworkAssetAccessor.cpp",
    "Implementations/NetworkAssetAccessor.h",
    "Implementations/InstrumentedCacheDatabase.cpp",
    "Implementations/InstrumentedCacheDatabase.h",
    "Implementations/GodotPrepareRenderResources.cpp",
    "Implementations/GodotPrepareRenderResources.h",
    "CesiumGDModelLoader.cpp",
    "CesiumGDModelLoader.h",
    "Utils/CesiumGDTextureLoader.cpp",
    "Utils/CesiumGDTextureLoader.h",
    "Models/CesiumGDCreditSystem.cpp",
    "Models/CesiumGDCreditSystem.h",
    "Models/CesiumGlobe.cpp",
    "Models/CesiumGlobe.h",
    "Models/GeoreferencedNode.cpp",
    "Models/GeoreferencedNode.h",
)

STALE_INCLUDE_FRAGMENTS = (
    '"Models/CesiumGDTileset.h"',
    '"Models/CesiumGDRasterOverlay.h"',
    '"Implementations/GodotTaskProcessor.h"',
    '"Implementations/NetworkAssetAccessor.h"',
    '"Implementations/InstrumentedCacheDatabase.h"',
    '"Implementations/GodotPrepareRenderResources.h"',
    '"../Implementations/NetworkAssetAccessor.h"',
    '"../Implementations/GodotPrepareRenderResources.h"',
    '"Utils/CesiumGDTextureLoader.h"',
    '"../Utils/CesiumGDTextureLoader.h"',
    '"../CesiumGDModelLoader.h"',
    '"Models/CesiumGDCreditSystem.h"',
    '"Models/CesiumGlobe.h"',
    '"Models/GeoreferencedNode.h"',
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    for relative in MIGRATED_FILES:
        path = SOURCE_ROOT / relative
        require(path.is_file(), f"missing migrated source: {relative}")
        if path.suffix == ".h":
            source = path.read_text(encoding="utf-8-sig")
            require(
                "Last upstream review: Cesium for Unreal v2.29.0" in source,
                f"missing upstream-review marker: {relative}",
            )

    for relative in LEGACY_FILES:
        require(
            not (SOURCE_ROOT / relative).exists(),
            f"legacy path was recreated after migration: {relative}",
        )

    scsub = (SOURCE_ROOT / "SCsub").read_text(encoding="utf-8-sig")
    source_entries = re.findall(
        r'get_root_dir\(\) \+ "(/[^"]+\.(?:c|cpp))"',
        scsub,
    )
    require(source_entries, "SCsub source manifest could not be parsed")
    for entry in source_entries:
        require(
            (SOURCE_ROOT / entry[1:]).is_file(),
            f"SCsub references a missing source: {entry}",
        )

    for path in SOURCE_ROOT.rglob("*"):
        if path.suffix not in {".cpp", ".h"} or "third_party" in path.parts:
            continue
        source = path.read_text(encoding="utf-8-sig")
        for fragment in STALE_INCLUDE_FRAGMENTS:
            require(
                fragment not in source,
                f"stale migrated include {fragment} in {path.relative_to(REPOSITORY)}",
            )

    public_root = SOURCE_ROOT / "Runtime" / "Public"
    for path in public_root.rglob("*"):
        if path.suffix not in {".cpp", ".h"}:
            continue
        source = path.read_text(encoding="utf-8-sig")
        require(
            '#include "Runtime/Private/' not in source,
            f"public runtime API depends on a private header: {path.relative_to(REPOSITORY)}",
        )

    porting_map = (REPOSITORY / "PORTING_MAP.md").read_text(encoding="utf-8")
    for responsibility in (
        "Godot/Nodes/CesiumGDTileset",
		"Runtime/Public/CesiumCameraManager",
        "Runtime/Public/RasterOverlays/CesiumRasterOverlay",
		"Runtime/Public/RasterOverlays/CesiumDebugColorizeTilesRasterOverlay",
		"Runtime/Public/RasterOverlays/CesiumBingMapsRasterOverlay",
		"Runtime/Public/RasterOverlays/CesiumGoogleMapTilesRasterOverlay",
        "Runtime/Public/RasterOverlays/CesiumIonRasterOverlay",
        "Runtime/Public/RasterOverlays/CesiumTileMapServiceRasterOverlay",
        "Runtime/Public/RasterOverlays/CesiumUrlTemplateRasterOverlay",
        "Runtime/Public/RasterOverlays/CesiumWebMapServiceRasterOverlay",
        "Runtime/Public/RasterOverlays/CesiumWebMapTileServiceRasterOverlay",
        "Godot/Nodes/CesiumTileDebugVisualizer",
        "Godot/Nodes/CesiumGDCreditSystem",
        "Runtime/Private/Async/GodotTaskProcessor",
        "Runtime/Private/Networking/NetworkAssetAccessor",
        "Runtime/Public/Networking/CesiumUrlUtility",
        "Runtime/Private/Renderer/GodotPrepareRenderResources",
        "Runtime/Private/Renderer/CesiumGDModelLoader",
        "Runtime/Public/Diagnostics/CesiumTileDebugState",
        "Runtime/Private/Diagnostics/CesiumHardwareCapabilities",
        "Runtime/Public/Credits/CesiumCredit",
        "Runtime/Private/Georeference/CesiumGodotCameraProjection",
        "Runtime/Private/Georeference/CesiumCameraPredictor",
        "Godot/Nodes/CesiumGeoreference",
        "Godot/Nodes/CesiumGeoreferencedMesh",
        "Runtime/Public/Georeference/CesiumEllipsoid",
        "Runtime/Public/Georeference/CesiumGlobeAnchor",
        "Runtime/Public/Georeference/CesiumOriginShift",
        "Runtime/Public/Georeference/CesiumFlyTo",
        "Runtime/Public/Geocoder/CesiumGeocoderService",
		"Runtime/Public/Georeference/CesiumCartographicPolygon",
		"Godot/Nodes/CesiumTileExcluder",
		"Runtime/Public/TileSelection/CesiumTileExclusionContext",
		"Runtime/Public/RasterOverlays/CesiumPolygonRasterOverlay",
		"Runtime/Public/RasterOverlays/CesiumGeoJsonDocumentRasterOverlay",
		"Runtime/Public/Vector/CesiumGeoJsonDocument",
		"Runtime/Public/Vector/CesiumGeoJsonObject",
		"Runtime/Public/Vector/CesiumVectorStyle",
        "Runtime/Private/Materials/CesiumLodTransitionController",
    ):
        require(
            responsibility in porting_map,
            f"PORTING_MAP is missing migrated responsibility: {responsibility}",
        )

    print(
        "Cesium source-layout validation passed: "
        f"{len(MIGRATED_FILES)} migrated files, {len(source_entries)} build sources"
    )


if __name__ == "__main__":
    main()
