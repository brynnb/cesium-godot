using CesiumForGodot;
using Godot;
using Godot.Collections;
using System;
using System.IO;
using System.Threading.Tasks;

internal static class CSharpStreamingIntegrationTests
{
    private const int LoadTimeoutFrames = 600;
    private const int TransitionTimeoutFrames = 240;
    private const double EarthRadius = 6_378_137.0;

    public static async Task RunAsync(SceneTree tree)
    {
        var testRoot = new Node3D { Name = "CSharpStreamingIntegrationTest" };
        tree.Root.AddChild(testRoot);
        // SceneTree._Initialize runs before the root Window has entered the
        // active tree. Wait once so global transforms and Camera3D.look_at are
        // valid before driving Cesium selection.
        await tree.ToSignal(tree, SceneTree.SignalName.ProcessFrame);

        try
        {
            await TestLocalStreamingAndUnloadAsync(tree, testRoot);
            await TestTerminalLoadFailureAsync(tree, testRoot);
        }
        finally
        {
            if (GodotObject.IsInstanceValid(testRoot))
                testRoot.Free();
        }
    }

    private static async Task TestLocalStreamingAndUnloadAsync(SceneTree tree, Node3D testRoot)
    {
        var camera = new Camera3D
        {
            Name = "CSharpTestCamera",
            Current = true,
            Position = new Vector3((float)EarthRadius + 50.0f, 100.0f, 300.0f),
        };
        testRoot.AddChild(camera);
        camera.LookAt(new Vector3((float)EarthRadius + 50.0f, 0.0f, 50.0f), Vector3.Up);

        var georeference = new CesiumGeoreference
        {
            OriginType = (int)CesiumGeoreference.OriginTypeEnum.TrueOrigin,
        };
        georeference.NativeObject.Name = "CSharpLocalCoordinates";
        testRoot.AddChild(georeference.NativeObject);

        var receiver = new Cesium3DTilesetLifecycleEventReceiver();
        receiver.NativeObject.Name = "CSharpLifecycleReceiver";
        testRoot.AddChild(receiver.NativeObject);

        int materialSelections = 0;
        int primitiveLoads = 0;
        int tileLoads = 0;
        int visibleTransitions = 0;
        int hiddenTransitions = 0;
        int unloads = 0;
        int primitivesAtUnload = -1;
        Exception callbackFailure = null;
        Cesium3DTile loadedTile = null;

        receiver.MaterialSelector = Callable.From<GodotObject, Material, Material>(
            (nativePrimitive, defaultMaterial) =>
            {
                try
                {
                    Require(nativePrimitive != null, "material selector received no primitive");
                    Require(defaultMaterial != null, "material selector received no default material");
                    materialSelections++;
                }
                catch (Exception exception)
                {
                    callbackFailure = exception;
                }
                return defaultMaterial;
            }
        );
        receiver.TileMeshPrimitiveLoaded += nativePrimitive =>
        {
            try
            {
                var primitive = (CesiumLoadedTilePrimitive)Variant.From(nativePrimitive);
                Require(primitive != null, "primitive signal argument could not be wrapped");
                primitiveLoads++;
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
            }
        };
        receiver.TileLoaded += nativeTile =>
        {
            try
            {
                loadedTile = (Cesium3DTile)Variant.From(nativeTile);
                Require(loadedTile != null, "tile_loaded argument could not be wrapped");
                tileLoads++;
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
            }
        };
        receiver.TileVisibilityChanged += (nativeTile, visible) =>
        {
            try
            {
                Require(
                    (Cesium3DTile)Variant.From(nativeTile) != null,
                    "visibility signal argument could not be wrapped"
                );
                if (visible)
                    visibleTransitions++;
                else
                    hiddenTransitions++;
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
            }
        };
        receiver.TileUnloading += nativeTile =>
        {
            try
            {
                var unloadingTile = (Cesium3DTile)Variant.From(nativeTile);
                Require(
                    unloadingTile != null,
                    "tile_unloading ran after its tile became invalid"
                );
                primitivesAtUnload = unloadingTile.GetLoadedTilePrimitiveCount();
                unloads++;
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
            }
        };

        var tileset = new Cesium3DTileset
        {
            DataSource = (int)Cesium3DTileset.CesiumDataSource.Url,
            Url = GetLifecycleFixtureUrl(),
            CreatePhysicsMeshes = false,
            MaximumScreenSpaceError = 1.0f,
            MaximumSimultaneousTileLoads = 4,
            PreloadAncestors = false,
            PreloadSiblings = false,
        };
        tileset.NativeObject.Name = "CSharpLifecycleFixtureTileset";
        tileset.SetLifecycleEventReceiver(receiver);
        georeference.NativeObject.AddChild(tileset.NativeObject);

        bool becameVisible = await PumpUntilAsync(
            tree,
            tileset,
            camera,
            () => visibleTransitions > 0,
            LoadTimeoutFrames
        );
        ThrowCallbackFailure(callbackFailure);
        Require(becameVisible, "local 3D Tiles fixture did not load and become visible");
        Require(tileLoads == 1, $"expected one loaded tile, observed {tileLoads}");
        // The fixture places the same three-surface mesh twice, proving that
        // callbacks preserve both node placement and primitive identity.
        Require(primitiveLoads == 6, $"expected six loaded primitives, observed {primitiveLoads}");
        Require(materialSelections == 6, $"expected six material selections, observed {materialSelections}");
        Require(loadedTile != null, "tile lifecycle did not retain the loaded tile wrapper");
        Require(
            loadedTile.TileId == "triangle.gltf",
            $"unexpected loaded tile ID: {loadedTile.TileId}"
        );
        Require(loadedTile.LoadedTilePrimitives.Count == 6, "loaded primitive array was not populated");

        Dictionary extras = loadedTile.TileExtras.AsGodotDictionary();
        Require(
            (string)extras["placement_id"] == "chunk_n25_26:17:2",
            "tile extras did not cross the generated C# facade"
        );

        Dictionary statistics = tileset.GetStreamingStatistics();
        Require((int)statistics["loaded"] >= 1, "streaming statistics did not report the loaded tile");
        bool initialLoadingFinished = await PumpUntilAsync(
            tree,
            tileset,
            camera,
            tileset.IsInitialLoadingFinished,
            TransitionTimeoutFrames
        );
        Require(initialLoadingFinished, "tileset never reported initial loading completion");

        camera.Position = new Vector3(1_000_000.0f, 1_000_000.0f, 1_000_000.0f);
        camera.LookAt(new Vector3(2_000_000.0f, 1_000_000.0f, 1_000_000.0f), Vector3.Up);
        bool becameHidden = await PumpUntilAsync(
            tree,
            tileset,
            camera,
            () => hiddenTransitions > 0,
            TransitionTimeoutFrames
        );
        ThrowCallbackFailure(callbackFailure);
        Require(becameHidden, "moving the C# camera did not hide the local tile");

        tileset.NativeObject.Free();
        await tree.ToSignal(tree, SceneTree.SignalName.ProcessFrame);
        ThrowCallbackFailure(callbackFailure);
        Require(unloads == 1, $"expected one tile_unloading signal, observed {unloads}");
        Require(
            primitivesAtUnload == 6,
            $"tile_unloading did not expose its six live primitives: {primitivesAtUnload}"
        );
        Require(
            !GodotObject.IsInstanceValid(loadedTile.NativeObject),
            "unloaded C# tile wrapper still referenced a live renderer node"
        );

        receiver.NativeObject.Free();
        georeference.NativeObject.Free();
        camera.Free();
    }

    private static async Task TestTerminalLoadFailureAsync(SceneTree tree, Node3D testRoot)
    {
        var camera = new Camera3D
        {
            Name = "CSharpFailureCamera",
            Current = true,
            Position = new Vector3((float)EarthRadius + 50.0f, 100.0f, 300.0f),
        };
        testRoot.AddChild(camera);
        camera.LookAt(new Vector3((float)EarthRadius + 50.0f, 0.0f, 50.0f), Vector3.Up);

        var georeference = new CesiumGeoreference
        {
            OriginType = (int)CesiumGeoreference.OriginTypeEnum.TrueOrigin,
        };
        testRoot.AddChild(georeference.NativeObject);

        CesiumLoadFailure failure = null;
        Exception callbackFailure = null;
        var tileset = new Cesium3DTileset
        {
            DataSource = (int)Cesium3DTileset.CesiumDataSource.Url,
            Url = GetMissingFixtureUrl(),
            CreatePhysicsMeshes = false,
        };
        tileset.LoadFailure += nativeFailure =>
        {
            try
            {
                failure = (CesiumLoadFailure)Variant.From(nativeFailure);
                Require(failure != null, "load_failure argument could not be wrapped");
            }
            catch (Exception exception)
            {
                callbackFailure = exception;
            }
        };
        georeference.NativeObject.AddChild(tileset.NativeObject);

        bool failed = await PumpUntilAsync(
            tree,
            tileset,
            camera,
            () => failure != null,
            TransitionTimeoutFrames
        );
        ThrowCallbackFailure(callbackFailure);
        Require(failed, "missing local tileset did not emit a C# load_failure signal");
        Require(failure.Terminal, "missing local tileset failure was not terminal");
        Require(!failure.Retryable, "missing local tileset failure was unexpectedly retryable");
        Require(!string.IsNullOrEmpty(failure.Message), "load failure did not include a message");
        Require(!string.IsNullOrEmpty(failure.StageName), "load failure did not include a stage");

        tileset.NativeObject.Free();
        georeference.NativeObject.Free();
        camera.Free();
        await tree.ToSignal(tree, SceneTree.SignalName.ProcessFrame);
    }

    private static async Task<bool> PumpUntilAsync(
        SceneTree tree,
        Cesium3DTileset tileset,
        Camera3D camera,
        Func<bool> condition,
        int maximumFrames
    )
    {
        for (int frame = 0; frame < maximumFrames; frame++)
        {
            tileset.UpdateTileset(camera.GlobalTransform);
            await tree.ToSignal(tree, SceneTree.SignalName.ProcessFrame);
            if (condition())
                return true;
        }
        return false;
    }

    private static string GetLifecycleFixtureUrl()
    {
        string projectDirectory = ProjectSettings.GlobalizePath("res://");
        string fixturePath = Path.GetFullPath(
            Path.Combine(projectDirectory, "..", "godot", "fixtures", "lifecycle", "tileset.json")
        );
        Require(File.Exists(fixturePath), $"local lifecycle fixture is missing: {fixturePath}");
        return new Uri(fixturePath).AbsoluteUri;
    }

    private static string GetMissingFixtureUrl()
    {
        string projectDirectory = ProjectSettings.GlobalizePath("res://");
        string missingPath = Path.Combine(projectDirectory, "fixtures", "missing-tileset.json");
        Require(!File.Exists(missingPath), $"failure fixture unexpectedly exists: {missingPath}");
        return new Uri(missingPath).AbsoluteUri;
    }

    private static void ThrowCallbackFailure(Exception callbackFailure)
    {
        if (callbackFailure != null)
            throw new InvalidOperationException("C# lifecycle callback failed", callbackFailure);
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidOperationException(message);
    }
}
