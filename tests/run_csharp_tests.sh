#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_dir="${repo_root}/tests/csharp"
dotnet_bin="${DOTNET_BIN:-$(command -v dotnet || true)}"
godot_dotnet_bin="${GODOT_DOTNET_BIN:-}"

if [[ -z "${dotnet_bin}" ]]; then
  echo "A .NET 8 SDK is required (set DOTNET_BIN if it is not on PATH)." >&2
  exit 1
fi
if [[ -z "${godot_dotnet_bin}" ]]; then
  godot_dotnet_bin="$(command -v godot4-mono || command -v godot-mono || true)"
fi
if [[ -z "${godot_dotnet_bin}" || ! -x "${godot_dotnet_bin}" ]]; then
  echo "The Godot 4.6.3 .NET executable is required; set GODOT_DOTNET_BIN." >&2
  exit 1
fi

mkdir -p "${project_dir}/.godot"
cp "${project_dir}/extension_list.cfg" "${project_dir}/.godot/extension_list.cfg"

restore_args=()
if [[ -n "${GODOT_NUGET_SOURCE:-}" ]]; then
  restore_args+=(--source "${GODOT_NUGET_SOURCE}")
fi

"${dotnet_bin}" restore "${project_dir}/CesiumCSharpSmoke.csproj" \
  --nologo "${restore_args[@]}"
"${dotnet_bin}" build "${project_dir}/CesiumCSharpSmoke.csproj" \
  --nologo --no-restore

dotnet_dir="$(cd "$(dirname "${dotnet_bin}")" && pwd)"
PATH="${dotnet_dir}:${PATH}" \
XDG_DATA_HOME="${project_dir}/.test-user-data" \
  "${godot_dotnet_bin}" --headless --path "${project_dir}" \
    --script res://CSharpBindingSmoke.cs
