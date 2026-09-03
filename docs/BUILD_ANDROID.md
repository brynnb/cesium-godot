# Building for Android

Android support currently targets 64-bit ARM devices (`arm64-v8a`) and is
cross-compiled from Linux. It is a runtime target; Cesium for Godot's editor
workflow remains desktop-only and experimental.

The extension targets Android API 24, matching Godot 4.6's minimum Android
version. Godot 4.6.3 exports applications with target SDK 36 by default, so APK
packaging also needs the Android SDK platform and build tools selected by the
Godot editor or export preset.

## Requirements

- the Python, SCons, CMake, and Ninja versions in `dependencies.lock.json`;
- an Android SDK; and
- Android NDK `28.1.13356709` (r28b), matching the dependency lock.

Install the locked NDK with the Android command-line tools:

```bash
sdkmanager "ndk;28.1.13356709"
```

Then set `ANDROID_HOME` to the SDK root and build:

```bash
export ANDROID_HOME=/absolute/path/to/android-sdk
python3 tools/build_extension.py \
  --platform android \
  --arch arm64 \
  --jobs 4
```

Alternatively, set `ANDROID_NDK_ROOT` directly to the exact NDK directory.
The build validates the locked revision instead of silently accepting a
different NDK.

The distributable addon is written to
`godot3dtiles/addons/cesium_godot`. Its Android library is:

```text
lib/libGodot3DTiles.android.template_release.arm64.so
```

Copy the complete `cesium_godot` addon directory into the Android project's
`addons` directory. The `.gdextension` manifest already selects the Android
library for both debug and release exports. Enable the Android export preset's
Internet permission when the tileset or overlays use HTTP or HTTPS URLs; local
file tilesets do not require it.

## Scope and current limitations

The Android target uses the same Cesium Native streaming and rendering runtime
as the desktop builds. The legacy litehtml-based `DocumentContainer` control
is omitted because it is not used by the runtime credit system and its bundled
libraries are desktop-only. Applications should display credits through
`CesiumCreditSystem`, as on desktop.

The network adapter obtains Android's system certificate authorities through
Godot and writes the PEM bundle once to the app's private data directory for
Cesium Native's OpenSSL-backed HTTP client. HTTPS certificate verification
therefore stays enabled without shipping a stale certificate bundle in the
addon.

Android support is new. The Android CI job performs the complete ARM64
cross-build and verifies the resulting binary architecture, but broad
physical-device and GPU-driver coverage is still needed before treating it as
mature. Mobile projects should start with conservative cache, concurrency, and
screen-space-error settings.
