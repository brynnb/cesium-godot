# Lifecycle and material demo

This tiny project normally streams one local 3D Tiles 1.1 fixture and demonstrates every
`Cesium3DTilesetLifecycleEventReceiver` material decision and its tile,
visibility, raster, and unload signals. It contains no Vanguard code, Cesium
ion dependency, or credentials. Its dedicated Android emulator smoke mode also
loads the same public fixture over HTTPS to verify Android networking and
certificate handling.

Build the release extension from the repository root, then launch the example:

```bash
CESIUM_GODOT_NATIVE_ROOT=/absolute/path/to/compatible/cesium-native \
  scons -j8 arch=x64 compileTarget=extension target=template_release \
  buildCesium=no
godot4 --path examples/lifecycle_material_demo
```

The project-local `CesiumGodot.gdextension` points at the repository build
artifacts. A packaged copy of the example should instead copy the complete
`addons/cesium_godot` directory into its own `addons` directory.

The left panel shows the exact callback order. Its controls:

- attach or detach an application-owned raster texture;
- move the Cesium view away and back to generate visibility changes; and
- unload and recreate the tileset to demonstrate deterministic raster detach,
  tile unload, material recreation, and reload.

The custom receiver is deliberately small. It selects a `ShaderMaterial`,
applies per-primitive parameters, consumes the documented `demo_overlay_*`
shader convention, and only records bounded main-thread lifecycle work. The
fixture and generated checker texture are tiny so the example teaches the API,
not production tiling or art policy.

The example deliberately commits one 31-byte
`.godot/extension_list.cfg`. This is Godot's required extension registration
metadata, not a generated asset or cache; all other `.godot` import files stay
ignored. It makes the first launch reliable because Cesium's native classes
must be registered before Godot parses the typed demo scripts. The headless
regression mode is simply:

```bash
godot4 --headless --path examples/lifecycle_material_demo -- --smoke-test
```

It requires material selection/customization, primitive and tile completion,
visibility, raster attach/detach, and a valid pre-destruction unload signal
before exiting successfully.

Android exports use the same scene for a two-pass device test. The first pass
copies the packaged fixture to the application's private filesystem and loads
it through a canonical `file://` URL. The second pass loads the fixture over
HTTPS. Each pass renders briefly before unloading so the headless emulator
runner can capture a screenshot. See
[`docs/BUILD_ANDROID.md`](../../docs/BUILD_ANDROID.md) for the complete command.

The same example provides a fixed four-waypoint streaming benchmark:

```bash
godot4 --headless --path examples/lifecycle_material_demo -- --benchmark-route
godot4 --headless --path examples/lifecycle_material_demo -- \
  --benchmark-route --benchmark-output=/absolute/path/to/report.json
```

The JSON report includes frame and explicit tileset-update percentiles, queue
and memory high-water marks, visual transitions, failures, and bounded samples.
Use an absolute output path so generated reports remain visible and intentional.
