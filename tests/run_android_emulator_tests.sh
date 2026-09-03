#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$ROOT/build/android-sdk}}"
ANDROID_HOME="$ANDROID_SDK_ROOT"
ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-$ROOT/build/android-avd}"
ANDROID_EMULATOR_AVD="${ANDROID_EMULATOR_AVD:-cesium_godot_api35}"
ANDROID_EMULATOR_PORT="${ANDROID_EMULATOR_PORT:-5556}"
ANDROID_EMULATOR_GPU="${ANDROID_EMULATOR_GPU:-host}"
CESIUM_ANDROID_USE_XVFB="${CESIUM_ANDROID_USE_XVFB:-true}"
ANDROID_EMULATOR_SERIAL="emulator-$ANDROID_EMULATOR_PORT"
APK="${1:-$ROOT/build/android-smoke/cesium-godot-emulator-smoke.apk}"
OUTPUT_DIRECTORY="${CESIUM_ANDROID_TEST_OUTPUT:-$ROOT/build/android-smoke/emulator-results}"

export ANDROID_HOME ANDROID_SDK_ROOT ANDROID_AVD_HOME

ADB="$ANDROID_SDK_ROOT/platform-tools/adb"
AVDMANAGER="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/avdmanager"
EMULATOR="$ANDROID_SDK_ROOT/emulator/emulator"
SYSTEM_IMAGE="system-images;android-35;default;x86_64"
PACKAGE="org.cesiumgodot.runtime_smoke"

for required in "$ADB" "$AVDMANAGER" "$EMULATOR" "$APK"; do
	if [[ ! -e "$required" ]]; then
		echo "Android emulator test prerequisite is missing: $required" >&2
		exit 2
	fi
done

if [[ ! -d "$ANDROID_SDK_ROOT/system-images/android-35/default/x86_64" ]]; then
	echo "Install $SYSTEM_IMAGE with sdkmanager before running this test." >&2
	exit 2
fi

mkdir -p "$ANDROID_AVD_HOME" "$OUTPUT_DIRECTORY"
if [[ ! -f "$ANDROID_AVD_HOME/$ANDROID_EMULATOR_AVD.avd/config.ini" ]]; then
	printf 'no\n' | "$AVDMANAGER" create avd \
		--force \
		--name "$ANDROID_EMULATOR_AVD" \
		--package "$SYSTEM_IMAGE" \
		--device pixel_6
fi

if ss -ltn | awk '{print $4}' | grep -Eq "(^|:)$ANDROID_EMULATOR_PORT$"; then
	echo "Android emulator port $ANDROID_EMULATOR_PORT is already in use." >&2
	exit 2
fi

EMULATOR_LOG="$OUTPUT_DIRECTORY/emulator.log"
XVFB_PID=""
if [[ "$CESIUM_ANDROID_USE_XVFB" == "true" ]]; then
	if ! command -v Xvfb >/dev/null 2>&1; then
		echo "Xvfb is required for the headless Android rendering test." >&2
		exit 2
	fi
	for display_number in $(seq 90 110); do
		if [[ ! -e "/tmp/.X11-unix/X$display_number" ]]; then
			export DISPLAY=":$display_number"
			Xvfb "$DISPLAY" -screen 0 1280x800x24 -nolisten tcp \
				>"$OUTPUT_DIRECTORY/xvfb.log" 2>&1 &
			XVFB_PID=$!
			break
		fi
	done
	if [[ -z "$XVFB_PID" ]]; then
		echo "Could not allocate an isolated Xvfb display." >&2
		exit 2
	fi
	sleep 1
fi

emulator_arguments=(
	"@$ANDROID_EMULATOR_AVD"
	-port "$ANDROID_EMULATOR_PORT"
	-no-audio
	-no-boot-anim
	-no-snapshot
	-wipe-data
	-no-metrics
	-gpu "$ANDROID_EMULATOR_GPU"
)
if [[ "$CESIUM_ANDROID_USE_XVFB" != "true" ]]; then
	emulator_arguments+=( -no-window )
fi
"$EMULATOR" "${emulator_arguments[@]}" >"$EMULATOR_LOG" 2>&1 &
EMULATOR_PID=$!

cleanup() {
	"$ADB" -s "$ANDROID_EMULATOR_SERIAL" emu kill >/dev/null 2>&1 || true
	if kill -0 "$EMULATOR_PID" >/dev/null 2>&1; then
		kill "$EMULATOR_PID" >/dev/null 2>&1 || true
	fi
	wait "$EMULATOR_PID" >/dev/null 2>&1 || true
	if [[ -n "$XVFB_PID" ]] && kill -0 "$XVFB_PID" >/dev/null 2>&1; then
		kill "$XVFB_PID" >/dev/null 2>&1 || true
		wait "$XVFB_PID" >/dev/null 2>&1 || true
	fi
}
trap cleanup EXIT INT TERM

boot_deadline=$((SECONDS + 180))
boot_status=""
while (( SECONDS < boot_deadline )); do
	boot_status="$(
		"$ADB" -s "$ANDROID_EMULATOR_SERIAL" shell getprop sys.boot_completed \
			2>/dev/null | tr -d '\r' || true
	)"
	if [[ "$boot_status" == "1" ]]; then
		break
	fi
	sleep 1
done
if [[ "$boot_status" != "1" ]]; then
	echo "Android emulator did not finish booting. See $EMULATOR_LOG" >&2
	exit 1
fi
# Android may still apply its initial device overlay and rotate the display
# immediately after boot_completed. Launching during that window destroys a
# Godot activity before its first scene can start.
sleep 5
# Avoid Android's first-use immersive-mode explanation covering the rendered
# fixture. This changes only the disposable emulator created by this script.
"$ADB" -s "$ANDROID_EMULATOR_SERIAL" shell settings put secure \
	immersive_mode_confirmations confirmed

"$ADB" -s "$ANDROID_EMULATOR_SERIAL" install -r "$APK"

run_number=1
while (( run_number <= 2 )); do
	"$ADB" -s "$ANDROID_EMULATOR_SERIAL" shell am force-stop "$PACKAGE"
	"$ADB" -s "$ANDROID_EMULATOR_SERIAL" logcat -c
	"$ADB" -s "$ANDROID_EMULATOR_SERIAL" shell monkey \
		-p "$PACKAGE" -c android.intent.category.LAUNCHER 1 >/dev/null

	run_deadline=$((SECONDS + 60))
	screenshot_taken=false
	while (( SECONDS < run_deadline )); do
		logcat_output="$(
			"$ADB" -s "$ANDROID_EMULATOR_SERIAL" logcat -d -v brief \
				godot:I Godot:I AndroidRuntime:E libc:F '*:S'
		)"
		if [[ "$screenshot_taken" == false ]] && grep -q "CESIUM_ANDROID_RENDER_READY" <<<"$logcat_output"; then
			"$ADB" -s "$ANDROID_EMULATOR_SERIAL" shell screencap -p \
				"/sdcard/cesium-godot-smoke-$run_number.png"
			"$ADB" -s "$ANDROID_EMULATOR_SERIAL" pull \
				"/sdcard/cesium-godot-smoke-$run_number.png" \
				"$OUTPUT_DIRECTORY/run-$run_number.png" >/dev/null
			screenshot_taken=true
		fi
		if grep -q "CESIUM_ANDROID_SMOKE_PASSED" <<<"$logcat_output"; then
			printf '%s\n' "$logcat_output" >"$OUTPUT_DIRECTORY/run-$run_number.log"
			break
		fi
		if grep -Eq "CESIUM_SMOKE_FAILURE|FATAL EXCEPTION|Fatal signal|No GDExtension library found|Could not load library|QueuePresentKHR failed" <<<"$logcat_output"; then
			printf '%s\n' "$logcat_output" >"$OUTPUT_DIRECTORY/run-$run_number.log"
			echo "Android app crashed or failed to load the extension on run $run_number." >&2
			exit 1
		fi
		sleep 1
	done

	if ! grep -q "CESIUM_ANDROID_SMOKE_PASSED" <<<"$logcat_output"; then
		printf '%s\n' "$logcat_output" >"$OUTPUT_DIRECTORY/run-$run_number.log"
		echo "Android smoke run $run_number timed out. See $OUTPUT_DIRECTORY/run-$run_number.log" >&2
		exit 1
	fi
	for marker in \
		CESIUM_ANDROID_PACKAGED_FIXTURE_COPIED \
		CESIUM_ANDROID_LOCAL_SMOKE_PASSED \
		CESIUM_ANDROID_HTTPS_SMOKE_PASSED; do
		if ! grep -q "$marker" <<<"$logcat_output"; then
			echo "Android smoke run $run_number did not report $marker." >&2
			exit 1
		fi
	done
	if [[ "$screenshot_taken" != true ]]; then
		echo "Android smoke run $run_number never reached a render-ready state." >&2
		exit 1
	fi
	echo "Android smoke run $run_number passed."
	run_number=$((run_number + 1))
done

echo "Cesium for Godot Android emulator tests passed."
