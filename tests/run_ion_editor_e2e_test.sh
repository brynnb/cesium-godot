#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
godot_bin="${GODOT_BIN:-godot4}"
project_dir="${repo_root}/tests/godot"
test_dir="$(mktemp -d --tmpdir=/var/tmp cesium-ion-e2e.XXXXXX)"
server_pid=""

cleanup() {
	if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
		kill "${server_pid}"
		wait "${server_pid}" 2>/dev/null || true
	fi
	find "${test_dir}" -depth -delete
}
trap cleanup EXIT

python3 "${repo_root}/tests/ion_editor_e2e_fixture.py" serve \
	--port-file "${test_dir}/port" \
	--events-file "${test_dir}/events.jsonl" &
server_pid=$!
for _ in {1..100}; do
	[[ -s "${test_dir}/port" ]] && break
	sleep 0.05
done
if [[ ! -s "${test_dir}/port" ]]; then
	echo "ion fixture did not start" >&2
	exit 1
fi

mkdir -p "${test_dir}/godot-data" "${project_dir}/.godot"
cp "${project_dir}/extension_list.cfg" "${project_dir}/.godot/extension_list.cfg"
port="$(<"${test_dir}/port")"
browser="python3 ${repo_root}/tests/ion_editor_e2e_fixture.py browse %s"
set +e
XDG_DATA_HOME="${test_dir}/godot-data" \
	BROWSER="${browser}" \
	CESIUM_ION_FIXTURE_URL="http://127.0.0.1:${port}/" \
	"${godot_bin}" --headless --editor --path "${project_dir}" \
		--script res://ion_editor_session_e2e_test.gd
godot_status=$?
set -e
if [[ ${godot_status} -ne 0 ]]; then
	echo "Ion fixture events before Godot failure:" >&2
	cat "${test_dir}/events.jsonl" >&2
	exit "${godot_status}"
fi

python3 - "${test_dir}/events.jsonl" <<'PY'
import json
from pathlib import Path
import sys

events = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
names = [event["event"] for event in events]
required = ["app_data", "authorize", "token_exchange", "profile", "assets", "tokens", "create_token"]
if names[:len(required)] != required:
    raise SystemExit(f"unexpected ion E2E event sequence: {names}")
if names[len(required):] not in ([], ["app_data", "profile"]):
    raise SystemExit(f"unexpected ion session-resume sequence: {names}")
if not next(event for event in events if event["event"] == "token_exchange")["pkce_valid"]:
    raise SystemExit("ion E2E token exchange did not validate PKCE")
if not next(event for event in events if event["event"] == "create_token")["request_valid"]:
    raise SystemExit("ion E2E token creation request was malformed")
print("Cesium ion E2E fixture verified PKCE and authenticated account operations")
PY
