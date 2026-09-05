# Basic editor workflow

Enable the Cesium Godot plugin and open a scene before adding content.

- **Add tileset:** paste an HTTP(S) URL or a local path. **Browse tileset / catalog**
  opens a local tileset or v1 catalog. Local absolute paths refer to this machine;
  they do not automatically package external content into an exported game.
- **Set up ordinary camera:** adds a georeference if needed, Camera3D,
  CesiumCameraManager, and a small `CesiumStreaming` scene driver. Position the
  camera and configure the georeference for your data; arbitrary datasets do not
  share a useful universal starting camera position. Save the scene and press Play.
  Existing cameras, georeferences, and camera managers are reused by type, even
  after renaming. Multiple candidates are rejected without modifying the scene;
  advanced multi-camera/multi-georeference projects should wire these manually.
- **Undo/redo:** dock asset insertion (including imagery overlays) and camera
  setup are scene-history actions. Created nodes have explicit scene ownership.
- **Search:** filter ion assets by name and type, or browse loaded catalog entries.
  Unsupported ion asset types are displayed but cannot be inserted.
- **Status:** select a tileset to see its editor-state load percentage and visible
  tiles, missing configuration, and last load failure. Enable **Preview selected
  tileset** to stream using the first editor 3D viewport camera. This can download
  data. It is not telemetry from a separately running game.
- **Token:** save a read-only runtime asset token explicitly in project settings.
  This is separate from the securely stored editor login; see ION_AUTHORIZATION.md.

Applications that already call `update_tileset` should not add `CesiumStreaming`.
The optional legacy Cesium camera controllers drive their own selection loop;
the ordinary-camera driver skips them to avoid duplicate updates.

Editor integration remains experimental. There is no remote catalog transport,
automatic dataset framing, custom georeference gizmo, or comprehensive wizard.
The editor test suite checks insertion, undo/redo, scene serialization, dock
cleanup, ordinary-camera local streaming, and OAuth against a local test server.

`python tests/run_packaged_editor_test.py --godot /path/to/godot` constructs a ZIP,
installs it into a fresh isolated project, runs editor actions, saves the scene,
and loads that saved scene in a second runtime process. CI runs this on Linux
and Windows at both supported Godot endpoints. `--rendered` also saves a runtime
screenshot; on Linux use a private X server such as `xvfb-run -a`.
