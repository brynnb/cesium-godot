#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
godot_bin="${GODOT_BIN:-godot4}"
project_dir="${repo_root}/tests/godot-editor"
import_dir="${project_dir}/.godot"
test_data_dir="${project_dir}/.test-user-data"
success_marker="Cesium editor dock registration test passed"

cleanup() {
  if [[ -d "${import_dir}" ]]; then
    find "${import_dir}" -depth -delete
  fi
  if [[ -d "${test_data_dir}" ]]; then
    find "${test_data_dir}" -depth -delete
  fi
}
trap cleanup EXIT

mkdir -p "${import_dir}" "${test_data_dir}"
cp "${project_dir}/extension_list.cfg" "${import_dir}/extension_list.cfg"
output_file="$(mktemp --tmpdir="${test_data_dir}" editor-smoke.XXXXXX.log)"
camera_output="${test_data_dir}/camera.log"
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless --path "${project_dir}" \
  --quit-after 6000 -- --smoke-test >"${camera_output}" 2>&1
cat "${camera_output}"
if ! grep -Fq "Ordinary Camera3D streaming fixture passed" "${camera_output}"; then
  echo "Ordinary Camera3D fixture failed" >&2
  exit 1
fi

set +e
XDG_DATA_HOME="${test_data_dir}" "${godot_bin}" --headless --editor \
	--path "${project_dir}" --quit-after 600 \
	>"${output_file}" 2>&1
status=$?
set -e
cat "${output_file}"

if [[ ${status} -ne 0 ]]; then
  echo "Cesium editor dock registration test failed with exit code ${status}" >&2
  exit "${status}"
fi
if ! grep -Fq "${success_marker}" "${output_file}"; then
  echo "Cesium editor dock registration test did not report success" >&2
  exit 1
fi
if grep -Fq "SCRIPT ERROR:" "${output_file}"; then
  echo "Cesium editor plugin reported a script error" >&2
  exit 1
fi
if grep -Fq "Invalid access to property or key 'items'" "${output_file}"; then
	echo "Cesium editor plugin tried to consume an invalid ion response" >&2
	exit 1
fi

GODOT_BIN="${godot_bin}" "${repo_root}/tests/run_ion_editor_e2e_test.sh"
