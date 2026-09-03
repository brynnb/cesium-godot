using CesiumForGodot;
using Godot;
using Godot.Collections;
using System;

internal static class CSharpFacadeTests
{
    public static void RunAll()
    {
        TestConstructionPropertiesAndMethods();
        TestArraysEnumsAndResources();
        TestSignals();
        TestCallableCallbacks();
        TestErrorResultsAndValidation();
        TestCrossPlatformFileUrls();
    }

    private static void TestCrossPlatformFileUrls()
    {
        Require(
            CesiumUrlUtility.LocalPathToFileUrl(@"C:\World Data\Kōjan tileset.json") ==
                "file:///C:/World%20Data/K%C5%8Djan%20tileset.json",
            "Windows drive path did not become a canonical file URL"
        );
        Require(
            CesiumUrlUtility.LocalPathToFileUrl(@"\\server\share name\tileset.json") ==
                "file://server/share%20name/tileset.json",
            "UNC path did not become a canonical file URL"
        );
    }

    private static void TestConstructionPropertiesAndMethods()
    {
        var tileset = new Cesium3DTileset();
        var ellipsoid = new CesiumEllipsoid();
        var bounds = new CesiumBoundingVolume();

        Require(tileset.NativeObject is Node3D, "tileset did not wrap a Node3D");
        Require(ellipsoid.NativeObject is Resource, "ellipsoid did not wrap a Resource");
        Require(bounds.NativeObject is RefCounted, "bounds did not wrap a RefCounted");

        tileset.MaximumScreenSpaceError = 23.5f;
        Require(
            Mathf.IsEqualApprox(tileset.MaximumScreenSpaceError, 23.5f),
            "tileset property did not round-trip"
        );
        tileset.SetMaximumSimultaneousTileLoads(11);
        Require(
            tileset.GetMaximumSimultaneousTileLoads() == 11,
            "tileset method argument or return value did not round-trip"
        );
        Require(!bounds.IsValid(), "default bounding volume should be invalid");

        tileset.NativeObject.Free();
    }

    private static void TestArraysEnumsAndResources()
    {
        var georeference = new CesiumGeoreference();
        var ellipsoid = new CesiumEllipsoid();
        ellipsoid.SetRadiiPrecise(new[] { 10.0, 20.0, 30.0 });
        double[] radii = ellipsoid.GetRadiiPrecise();
        Require(
            radii.Length == 3 && radii[0] == 10.0 && radii[1] == 20.0 && radii[2] == 30.0,
            "managed numeric array did not round-trip"
        );

        georeference.Ellipsoid = ellipsoid;
        Require(
            georeference.Ellipsoid.NativeObject == ellipsoid.NativeObject,
            "extension Resource property did not preserve identity"
        );

        var style = new CesiumMetadataStyle();
        var rules = new Array<Dictionary>
        {
            new Dictionary
            {
                ["property"] = "feature_id",
                ["operator"] = "equals",
                ["value"] = 7,
                ["show"] = false,
                ["color_mix"] = 0.25,
            },
        };
        style.Rules = rules;
        Array<Dictionary> returnedRules = style.Rules;
        Require(
            returnedRules.Count == 1 && (string)returnedRules[0]["property"] == "feature_id",
            "typed Godot array property did not round-trip"
        );
        Dictionary evaluated = style.EvaluateFeature(7, new Dictionary());
        Require(
            (bool)evaluated["matched"] && !(bool)evaluated["show"],
            "array-backed metadata rule was not consumed by a generated method"
        );

        var tile = new Cesium3DTile();
        Require(
            tile.LoadedTilePrimitives.Count == 0,
            "untyped extension-object array did not marshal as an empty Godot array"
        );

        var overlay = new CesiumGoogleMapTilesRasterOverlay();
        overlay.Scale = (int)CesiumGoogleMapTilesRasterOverlay.ScaleEnum.Value2x;
        Require(
            overlay.Scale == (int)CesiumGoogleMapTilesRasterOverlay.ScaleEnum.Value2x,
            "numeric enum binding did not round-trip"
        );
        style.FeatureSource = (int)CesiumMetadataStyle.FeatureSourceEnum.InstanceFeatures;
        Require(
            style.FeatureSource == (int)CesiumMetadataStyle.FeatureSourceEnum.InstanceFeatures,
            "named enum binding did not round-trip"
        );

        tile.NativeObject.Free();
        overlay.NativeObject.Free();
        georeference.NativeObject.Free();
    }

    private static void TestSignals()
    {
        var georeference = new CesiumGeoreference();
        int changedCount = 0;
        Action<int> onChanged = _revision => changedCount++;
        georeference.GeoreferenceChanged += onChanged;
        georeference.SetOriginLongitudeLatitudeHeightPrecise(new[] { -75.0, 40.0, 125.0 });
        double[] origin = georeference.GetOriginLongitudeLatitudeHeightPrecise();
        Require(
            origin.Length == 3 && origin[0] == -75.0 && origin[1] == 40.0 && origin[2] == 125.0,
            "signal-producing method did not preserve its array value"
        );
        Require(changedCount == 1, "generated signal subscription did not fire");
        georeference.GeoreferenceChanged -= onChanged;
        georeference.SetOriginLongitudeLatitudeHeightPrecise(new[] { -74.0, 41.0, 126.0 });
        Require(changedCount == 1, "generated signal subscription was not removed");

        bool objectArgumentWrapped = false;
        Action<GodotObject> onEllipsoidChanged = nativeEllipsoid =>
        {
            var wrapped = (CesiumEllipsoid)Variant.From(nativeEllipsoid);
            objectArgumentWrapped = wrapped?.NativeObject == nativeEllipsoid;
        };
        georeference.EllipsoidChanged += onEllipsoidChanged;
        var ellipsoid = new CesiumEllipsoid();
        georeference.Ellipsoid = ellipsoid;
        Require(objectArgumentWrapped, "extension-object signal argument was not wrappable");
        georeference.EllipsoidChanged -= onEllipsoidChanged;

        georeference.NativeObject.Free();
    }

    private static void TestCallableCallbacks()
    {
        var receiver = new Cesium3DTilesetLifecycleEventReceiver();
        var primitive = new CesiumLoadedTilePrimitive();
        var selectedMaterial = new StandardMaterial3D();
        int selectionCount = 0;
        receiver.MaterialSelector = Callable.From<GodotObject, Material, Material>(
            (_primitive, defaultMaterial) =>
            {
                selectionCount++;
                return defaultMaterial;
            }
        );
        Variant selected = receiver.MaterialSelector.Call(primitive.NativeObject, selectedMaterial);
        Require(selectionCount == 1, "material selector Callable did not run");
        Require(
            selected.AsGodotObject() == selectedMaterial,
            "material selector Callable did not return its material"
        );

        int customizingCount = 0;
        Action<GodotObject, Material> onCustomizing = (_primitive, material) =>
        {
            Require(material == selectedMaterial, "material signal changed its argument");
            customizingCount++;
        };
        receiver.MaterialCustomizing += onCustomizing;
        receiver.NativeObject.EmitSignal(
            Cesium3DTilesetLifecycleEventReceiver.SignalName.MaterialCustomizing,
            primitive.NativeObject,
            selectedMaterial
        );
        Require(customizingCount == 1, "material customizing signal did not fire");
        receiver.MaterialCustomizing -= onCustomizing;

        var exclusionContext = new CesiumTileExclusionContext();
        var excluder = new CesiumTileExcluder();
        int predicateCount = 0;
        excluder.Predicate = Callable.From<GodotObject, bool>(_context =>
        {
            predicateCount++;
            return true;
        });
        Variant excluded = excluder.Predicate.Call(exclusionContext.NativeObject);
        Require(predicateCount == 1 && excluded.AsBool(), "boolean decision Callable failed");

        excluder.NativeObject.Free();
        receiver.NativeObject.Free();
    }

    private static void TestErrorResultsAndValidation()
    {
        var excluder = new CesiumTileExcluder();
        var firstTileset = new Cesium3DTileset();
        var secondTileset = new Cesium3DTileset();

        excluder.Enabled = false;
        Require(
            excluder.AddToTileset(firstTileset) == Error.Unavailable,
            "disabled excluder did not return Error.Unavailable"
        );
        excluder.Enabled = true;
        Require(
            excluder.AddToTileset(firstTileset) == Error.Ok,
            "enabled excluder did not attach successfully"
        );
        Require(
            excluder.AddToTileset(secondTileset) == Error.AlreadyInUse,
            "conflicting attachment did not return Error.AlreadyInUse"
        );
        excluder.RemoveFromTileset(firstTileset);

        var style = new CesiumMetadataStyle();
        style.FeatureIdSetIndex = -10;
        style.DefaultColorMix = 4.0f;
        Require(style.FeatureIdSetIndex == 0, "invalid feature index was not clamped");
        Require(Mathf.IsEqualApprox(style.DefaultColorMix, 1.0f), "invalid color mix was not clamped");

        Variant nil = default;
        Require(
            (CesiumEllipsoid)nil == null,
            "null Variant conversion did not return a null wrapper"
        );

        excluder.NativeObject.Free();
        firstTileset.NativeObject.Free();
        secondTileset.NativeObject.Free();
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
