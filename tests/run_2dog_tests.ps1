param(
  [string]$DotNetBin = "dotnet"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceProject = Join-Path $PSScriptRoot "csharp-2dog"
$stageRoot = Join-Path $repoRoot "build/tests/csharp-2dog-windows"
$projectDir = Join-Path $stageRoot "csharp-2dog"
$testProject = Join-Path $projectDir "CesiumCSharpTwoDog.Tests/CesiumCSharpTwoDog.Tests.csproj"
$addonSource = Join-Path $repoRoot "addons/cesium_godot"

# Windows Git checkouts do not reliably materialize repository symlinks. Stage
# the fixture and freshly-built addon explicitly in an ignored build directory.
if (Test-Path $stageRoot) {
  Remove-Item -Recurse -Force $stageRoot
}
New-Item -ItemType Directory -Force `
  $projectDir, `
  (Join-Path $projectDir "CesiumCSharpTwoDog.Tests"), `
  (Join-Path $projectDir "addons"), `
  (Join-Path $stageRoot "csharp"), `
  (Join-Path $stageRoot "godot/fixtures") | Out-Null

Get-ChildItem -Force -File $sourceProject | Copy-Item -Destination $projectDir
Get-ChildItem -Force -File (Join-Path $sourceProject "CesiumCSharpTwoDog.Tests") |
  Copy-Item -Destination (Join-Path $projectDir "CesiumCSharpTwoDog.Tests")
Copy-Item `
  (Join-Path $PSScriptRoot "csharp/CSharpFacadeTests.cs"), `
  (Join-Path $PSScriptRoot "csharp/CSharpStreamingIntegrationTests.cs") `
  -Destination (Join-Path $stageRoot "csharp")
Copy-Item -Recurse -Force `
  (Join-Path $PSScriptRoot "godot/fixtures/lifecycle") `
  (Join-Path $stageRoot "godot/fixtures/lifecycle")
Copy-Item -Recurse -Force $addonSource (Join-Path $projectDir "addons/cesium_godot")

$importDir = Join-Path $projectDir ".godot"
New-Item -ItemType Directory -Force $importDir | Out-Null
Copy-Item (Join-Path $projectDir "extension_list.cfg") $importDir

$env:TWODOG_GODOT_LOG_DIR = Join-Path $stageRoot "logs"
New-Item -ItemType Directory -Force $env:TWODOG_GODOT_LOG_DIR | Out-Null

& $DotNetBin restore $testProject `
  --nologo `
  --locked-mode `
  --verbosity minimal
if ($LASTEXITCODE -ne 0) {
  throw "Windows 2dog restore failed with exit code $LASTEXITCODE"
}

& $DotNetBin test $testProject `
  --nologo `
  --configuration Debug `
  --no-restore `
  --blame-hang `
  --blame-hang-timeout 5m `
  --verbosity minimal
if ($LASTEXITCODE -ne 0) {
  throw "Windows 2dog/xUnit test failed with exit code $LASTEXITCODE"
}
