# Downstream Native patches

The ordered series and exact hashes live in `../../dependencies.lock.json`.
Do not edit an earlier patch to implement a new change; append a patch and
update the expected final tree. Bootstrap verifies both hashes and tree.

## 0014: glTF cancellation and shared request ownership

Cancellation is control flow, not damaged content. SharedAssetDepot converts
request exceptions to ErrorList values; previously a canceled image request
could reach glTF post-processing as an image with neither a URI nor bytes,
producing misleading corrupt-image errors before the content manager discarded
the canceled tile.

The patch propagates the manager's token through a per-load content-options
snapshot, checks it before external-result interpretation/post-processing and
loader logging, and keeps depot-owned image/schema requests independent of an
individual tile's token. Private requests still use the cancellation wrapper.
The manager's existing canceled-load path returns the tile to Unloaded without
an error callback, so it can be requested again normally.

TileLoadInput owns its options and accessor by value: deferred loader work must
not retain references to the manager's temporary per-load context/wrapper.

## 0015: cancelable ion OAuth authorization

Cesium Native's maintained ion login already provides a random loopback port,
PKCE, and state validation, but its listener could not be stopped until the
browser redirected back. The Godot editor must be able to cancel sign-in and
unload its plugin cleanly, so this patch returns an idempotent cancellation
handle alongside the authorization future. The original `authorize` API remains
source-compatible and delegates to the cancelable operation.

Focused regressions (in a `CESIUM_TESTS_ENABLED=ON` build):

```sh
CesiumNativeTests/cesium-native-tests --source-file='*TestOAuth2PKCE.cpp,*TestGltfCancellation.cpp,*TestTilesetJsonLoader.cpp,*TestGltfReader.cpp,*TestImplicitQuadtreeLoader.cpp,*TestImplicitOctreeLoader.cpp,*TestTilesetContentManager.cpp,*TestTileLoadRequester.cpp'
```

Coverage includes canceled image error conversion, retry, simultaneous shared
image consumers, genuine invalid PNG data, cancellation before image decode,
request-context lifetime, idempotent OAuth cancellation, PKCE/state URL
construction, and existing glTF/tile loader behavior. Shared fetches
may finish after all their current tiles cancel; that bounded cache/depot work
is intentional rather than canceling another consumer's request.
