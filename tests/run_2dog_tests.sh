#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_dir="${repo_root}/tests/csharp-2dog"
test_project="${project_dir}/CesiumCSharpTwoDog.Tests/CesiumCSharpTwoDog.Tests.csproj"
dotnet_bin="${DOTNET10_BIN:-$(command -v dotnet || true)}"

if [[ -z "${dotnet_bin}" ]]; then
  echo "A .NET 10 SDK is required (set DOTNET10_BIN if dotnet is not on PATH)." >&2
  exit 1
fi

dotnet_major="$(${dotnet_bin} --version | cut -d. -f1)"
if [[ "${dotnet_major}" -lt 10 ]]; then
  echo "The optional 2dog test requires .NET 10; found $(${dotnet_bin} --version)." >&2
  exit 1
fi

mkdir -p "${project_dir}/.godot" "${repo_root}/build/logs/2dog"
cp "${project_dir}/extension_list.cfg" "${project_dir}/.godot/extension_list.cfg"
dotnet_dir="$(cd "$(dirname "${dotnet_bin}")" && pwd)"

PATH="${dotnet_dir}:${PATH}" \
TWODOG_GODOT_LOG_DIR="${repo_root}/build/logs/2dog" \
  "${dotnet_bin}" restore "${test_project}" \
    --nologo \
    --locked-mode \
    --verbosity minimal

PATH="${dotnet_dir}:${PATH}" \
TWODOG_GODOT_LOG_DIR="${repo_root}/build/logs/2dog" \
  "${dotnet_bin}" test "${test_project}" \
    --nologo \
    --configuration Debug \
    --no-restore \
    --blame-hang \
    --blame-hang-timeout 5m \
    --verbosity minimal
