param(
    [ValidateSet("vs2022-debug", "vs2022-debug-tests", "vs2022-release-tests")]
    [string]$Preset = "vs2022-debug"
)

$ErrorActionPreference = "Stop"

$buildDirByPreset = @{
    "vs2022-debug" = "build/debug"
    "vs2022-debug-tests" = "build/debug-tests"
    "vs2022-release-tests" = "build/release-tests"
}

$buildDir = $buildDirByPreset[$Preset]
Write-Host "Removing '$buildDir'..."

if (Test-Path $buildDir) {
    Remove-Item -Path $buildDir -Recurse -Force
}

Write-Host "Configuring with preset '$Preset'..."
cmake --preset $Preset

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Configuration completed in '$buildDir'."
