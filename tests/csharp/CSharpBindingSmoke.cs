using CesiumForGodot;
using Godot;
using System;

public partial class CSharpBindingSmoke : SceneTree
{
    public override void _Initialize()
    {
        try
        {
            var tileset = new Cesium3DTileset();
            Require(tileset.NativeObject is Node3D, "tileset did not wrap a Node3D");
            tileset.MaximumScreenSpaceError = 23.5f;
            Require(
                Mathf.IsEqualApprox(tileset.MaximumScreenSpaceError, 23.5f),
                "tileset property did not round-trip"
            );

            var georeference = new CesiumGeoreference();
            int signalCount = 0;
            Action<int> onGeoreferenceChanged = _revision => signalCount++;
            georeference.GeoreferenceChanged += onGeoreferenceChanged;
            georeference.SetOriginLongitudeLatitudeHeightPrecise(
                new[] { -75.0, 40.0, 125.0 }
            );
            double[] origin = georeference.GetOriginLongitudeLatitudeHeightPrecise();
            Require(origin.Length == 3, "georeference origin result had the wrong size");
            Require(Mathf.IsEqualApprox(origin[0], -75.0), "longitude did not round-trip");
            Require(Mathf.IsEqualApprox(origin[1], 40.0), "latitude did not round-trip");
            Require(Mathf.IsEqualApprox(origin[2], 125.0), "height did not round-trip");
            Require(signalCount == 1, "generated signal subscription did not fire");
            georeference.GeoreferenceChanged -= onGeoreferenceChanged;
            georeference.SetOriginLongitudeLatitudeHeightPrecise(
                new[] { -74.0, 41.0, 126.0 }
            );
            Require(signalCount == 1, "generated signal subscription was not removed");

            bool ellipsoidSignalFired = false;
            Action<GodotObject> onEllipsoidChanged = nativeEllipsoid =>
            {
                var wrappedEllipsoid = (CesiumEllipsoid)Variant.From(nativeEllipsoid);
                ellipsoidSignalFired = wrappedEllipsoid != null;
            };
            georeference.EllipsoidChanged += onEllipsoidChanged;
            var ellipsoid = new CesiumEllipsoid();
            georeference.Ellipsoid = ellipsoid;
            Require(ellipsoidSignalFired, "extension-object signal argument was not wrapped");
            georeference.EllipsoidChanged -= onEllipsoidChanged;

            var overlay = new CesiumGoogleMapTilesRasterOverlay();
            overlay.Scale = (int)CesiumGoogleMapTilesRasterOverlay.ScaleEnum.Value2x;
            Require(
                overlay.Scale == (int)CesiumGoogleMapTilesRasterOverlay.ScaleEnum.Value2x,
                "numeric enum binding did not round-trip"
            );

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
            Variant selected = receiver.MaterialSelector.Call(
                primitive.NativeObject,
                selectedMaterial
            );
            Require(selectionCount == 1, "material selector Callable did not run");
            Require(
                selected.AsGodotObject() == selectedMaterial,
                "material selector Callable did not return its material"
            );

            int customizingCount = 0;
            Action<GodotObject, Material> onMaterialCustomizing =
                (_primitive, material) =>
                {
                    Require(material == selectedMaterial, "material signal changed its argument");
                    customizingCount++;
                };
            receiver.MaterialCustomizing += onMaterialCustomizing;
            receiver.NativeObject.EmitSignal(
                Cesium3DTilesetLifecycleEventReceiver.SignalName.MaterialCustomizing,
                primitive.NativeObject,
                selectedMaterial
            );
            Require(customizingCount == 1, "material customizing signal did not fire");
            receiver.MaterialCustomizing -= onMaterialCustomizing;

            receiver.NativeObject.Free();
            overlay.NativeObject.Free();
            georeference.NativeObject.Free();
            tileset.NativeObject.Free();
            GD.Print("Cesium C# binding smoke test passed");
            Quit(0);
        }
        catch (System.Exception exception)
        {
            GD.PushError($"Cesium C# binding smoke test failed: {exception}");
            Quit(1);
        }
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new System.InvalidOperationException(message);
    }
}
