# Building for Android

Android release support targets 64-bit ARM devices (`arm64-v8a`) and is
cross-compiled from Linux. An Android x86_64 build is also available for local
emulator testing; it is a test target, not a release artifact. Android is a
runtime target, while Cesium for Godot's editor workflow remains desktop-only
and experimental.

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

Use `--arch x86_64` instead to build the emulator-only library:

```bash
python3 tools/build_extension.py \
  --platform android \
  --arch x86_64 \
  --jobs 4
```

The distributable addon is written to
`addons/cesium_godot`. Its Android library is:

```text
lib/libCesiumGodot.android.template_release.arm64.so
```

Copy the complete `cesium_godot` addon directory into the Android project's
`addons` directory. The `.gdextension` manifest already selects the Android
library for both debug and release exports. Enable the Android export preset's
Internet permission when the tileset or overlays use HTTP or HTTPS URLs; local
file tilesets do not require it.

## Headless emulator smoke test

The repository has an end-to-end Android test using the official Android
Emulator, KVM, and an isolated Xvfb display. It never opens a window on the
desktop. The test installs and launches a real APK twice and verifies:

- Android loads the native x86_64 GDExtension;
- a packaged local tileset streams, renders, and unloads;
- the same tileset streams over HTTPS with certificate verification enabled;
- lifecycle and material callbacks cross the Godot/Cesium boundary; and
- the application can stop and relaunch without stale process state.

Install the emulator and its Android 35 x86_64 image alongside the locked NDK:

```bash
sdkmanager \
  "platform-tools" \
  "emulator" \
  "platforms;android-35" \
  "build-tools;35.0.1" \
  "system-images;android-35;default;x86_64"
```

After building the x86_64 extension, export the `Android Emulator Smoke`
preset in `examples/lifecycle_material_demo` as a debug APK. Then run:

```bash
ANDROID_HOME=/absolute/path/to/android-sdk \
  tests/run_android_emulator_tests.sh
```

The runner expects
`build/android-smoke/cesium-godot-emulator-smoke.apk` by default. Pass another
APK as its first argument if needed. Logs and screenshots are written under
`build/android-smoke/emulator-results`. `ANDROID_EMULATOR_GPU=host` is the
default because it exercises actual rendering; a software-only emulator may
still validate callbacks but is not equivalent GPU-driver coverage.

This test does not replace physical-device testing. It cannot establish ARM64
ABI correctness, mobile-GPU performance, thermals, memory pressure, or vendor
driver behavior.

## Scope and current limitations

The Android target uses the same Cesium Native streaming, rendering, and
Godot-native credit presentation runtime as the desktop builds.

The network adapter obtains Android's system certificate authorities through
Godot and writes the PEM bundle once to the app's private data directory for
Cesium Native's OpenSSL-backed HTTP client. HTTPS certificate verification
therefore stays enabled without shipping a stale certificate bundle in the
addon.

Android support is new. The Android CI job performs the complete ARM64
cross-build and verifies the resulting binary architecture. The local x86_64
emulator test covers APK startup, native-library loading, local and HTTPS tile
streaming, basic rendering, unloading, and relaunch. Broad physical-device and
GPU-driver coverage is still needed before treating Android as mature. Mobile
projects should start with conservative cache, concurrency, and
screen-space-error settings.
