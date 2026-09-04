# Cesium ion authorization

Cesium for Godot keeps two kinds of credentials deliberately separate:

- The **editor login session** lets the editor dock list the signed-in user's
  assets and create or select an asset token. `CesiumIonEditorSession` delegates
  this login to Cesium Native's maintained authorization flow, including a
  random loopback port, PKCE, state validation, refresh tokens, public ion,
  private-server application data, and single-user server mode.
- A **runtime asset token** is an application credential chosen by the project
  author. It remains explicit project/runtime configuration. Packaged games do
  not inherit the editor user's account login.

The editor access and refresh tokens are stored in Windows Credential Manager,
macOS Keychain, or the Linux Secret Service. If the platform vault is missing
or unavailable, login still works for the current editor process but is not
persisted. Cesium for Godot never falls back to a plaintext token file. The old
`user://cache/ion_session.dat` prototype file is deleted without reading it.

Disabling the plugin, canceling sign-in, or closing the editor cancels pending
network work and closes Cesium Native's temporary authorization listener. The
downstream Native patch that exposes this cancellation is recorded and locked
in `dependencies/cesium-native-patches`.

The Linux editor test suite runs the real GDExtension against a local ion/OAuth
fixture. It validates application discovery, the PKCE challenge and state-bound
loopback redirect, token exchange, profile validation, authenticated asset and
token operations, token creation, secure-vault persistence, fresh-session
resume, and sign-out cleanup without opening a real browser or requiring a
Cesium account. The persistence portion runs when a platform credential vault
is available.

## Application guidance

The editor account token is not exposed to GDScript or C#. Asset listing and
token management go through `CesiumIonEditorSession`, which uses Cesium
Native's refresh-aware `Connection` APIs. Use a separate, restricted Cesium ion
asset token for runtime tilesets and overlays, with only the assets and scopes
the packaged application requires. Never log either token.

The current editor dock targets the public Cesium ion application registration
by default. A private Cesium ion deployment can configure its server URL, API
URL, OAuth application ID, and redirect path on `CesiumIonEditorSession`.
