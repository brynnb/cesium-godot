param(
  [Parameter(Mandatory = $true)]
  [string]$GodotBin
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureSource = Join-Path $PSScriptRoot "godot-editor"
$projectDir = Join-Path $repoRoot "build/editor-smoke-windows"
$addonSource = Join-Path $repoRoot "addons/cesium_godot"
$addonDestination = Join-Path $projectDir "addons/cesium_godot"
$importDir = Join-Path $projectDir ".godot"
$successMarker = "Cesium editor dock registration test passed"

# Git symlinks are not reliably materialized on Windows runners. Copy the
# fixture and freshly-built distributable addon into an ignored build directory
# so this never mutates or follows the source tree's addon symlink.
if (Test-Path $projectDir) {
  Remove-Item -Recurse -Force $projectDir
}
New-Item -ItemType Directory -Force (Join-Path $projectDir "addons") | Out-Null
Copy-Item (Join-Path $fixtureSource "project.godot") $projectDir
Copy-Item -Recurse -Force `
  (Join-Path $fixtureSource "addons/editor_smoke_probe") `
  (Join-Path $projectDir "addons/editor_smoke_probe")
Copy-Item -Recurse -Force $addonSource $addonDestination

New-Item -ItemType Directory -Force $importDir | Out-Null
[IO.File]::WriteAllText(
  (Join-Path $importDir "extension_list.cfg"),
  "res://addons/cesium_godot/CesiumGodot.gdextension`n"
)

$output = & $GodotBin --headless --editor --path $projectDir --quit-after 1200 2>&1 |
  Tee-Object -Variable capturedOutput
$status = $LASTEXITCODE
$log = $capturedOutput -join "`n"

if ($status -ne 0) {
  throw "Cesium editor dock registration test failed with exit code $status"
}
if (-not $log.Contains($successMarker)) {
  throw "Cesium editor dock registration test did not report success"
}
if ($log.Contains("Invalid access to property or key 'items'")) {
  throw "Cesium editor plugin tried to consume an invalid ion response"
}
if ($log.Contains("Failed loading resource: res://addons/cesium_godot/resources/icons")) {
  throw "Cesium editor dock still depends on imported icon resources"
}
