#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "${repo_root}/tests/validate_source_layout.py"
python3 "${repo_root}/tests/test_validate_tileset.py"
python3 "${repo_root}/tests/test_dependency_lock.py"
python3 "${repo_root}/tests/test_build_configuration.py"
python3 "${repo_root}/tests/test_csharp_bindings.py"
python3 "${repo_root}/tools/check_compatibility.py"
godot_bin="${GODOT_BIN:-godot4}"
project_dir="${repo_root}/tests/godot"
import_dir="${project_dir}/.godot"
test_data_dir="${project_dir}/.test-user-data"
generated_dir="${project_dir}/.test-generated"
example_dir="${repo_root}/examples/lifecycle_material_demo"
example_test_data_dir="${example_dir}/.test-user-data"
server_pid=""
godot_args=()
if [[ "${GODOT_TEST_VERBOSE:-0}" == "1" ]]; then
  godot_args+=(--verbose)
fi

cleanup() {
  if [[ -n "${server_pid}" ]]; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  if [[ -d "${import_dir}" ]]; then
    find "${import_dir}" -depth -delete
  fi
  if [[ -d "${test_data_dir}" ]]; then
    find "${test_data_dir}" -depth -delete
  fi
  if [[ -d "${generated_dir}" ]]; then
    find "${generated_dir}" -depth -delete
  fi
  if [[ -d "${example_test_data_dir}" ]]; then
    find "${example_test_data_dir}" -depth -delete
  fi
}
trap cleanup EXIT

mkdir -p "${import_dir}"
cp "${project_dir}/extension_list.cfg" "${import_dir}/extension_list.cfg"
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://lifecycle_streaming_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://true_origin_axis_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://geospatial_foundation_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://geojson_document_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://fly_to_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://cartographic_streaming_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://tile_exclusion_polygon_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://line_point_renderer_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://translucency_renderer_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://debug_color_overlay_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://compressed_content_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://structural_metadata_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://metadata_styling_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://gpu_instancing_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://bounds_queries_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://frustum_culling_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://occlusion_bridge_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://occlusion_engine_e2e_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://occlusion_tileset_e2e_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://multi_camera_selection_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://height_sampling_test.gd
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://shared_resource_accounting_test.gd
XDG_DATA_HOME="${example_test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" \
  --path "${example_dir}" -- --smoke-test
XDG_DATA_HOME="${example_test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" \
  --path "${example_dir}" -- --benchmark-route

mkdir -p "${generated_dir}"
port_file="${generated_dir}/slow-server-port.txt"
request_marker="${generated_dir}/slow-server-requests.txt"
server_log="${generated_dir}/slow-server.log"
python3 "${repo_root}/tests/slow_fixture_server.py" \
  --fixture "${project_dir}/fixtures/lifecycle/triangle.gltf" \
  --geojson-fixture "${project_dir}/fixtures/vector/overlay.geojson" \
  --marker "${request_marker}" \
  --port-file "${port_file}" >"${server_log}" 2>&1 &
server_pid="$!"
for _attempt in $(seq 1 100); do
  [[ -s "${port_file}" ]] && break
  sleep 0.02
done
if [[ ! -s "${port_file}" ]]; then
  echo "Delayed fixture server did not start" >&2
  exit 1
fi
server_port="$(<"${port_file}")"
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://lod_continuity_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://adversarial_lifecycle_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://load_failure_retry_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://raster_provider_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://geocoder_service_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
CESIUM_TEST_REQUEST_MARKER="${request_marker}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://geojson_overlay_test.gd
CESIUM_TEST_SERVER_URL="http://127.0.0.1:${server_port}" \
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless "${godot_args[@]}" --path "${project_dir}" \
  --script res://credits_attribution_test.gd
