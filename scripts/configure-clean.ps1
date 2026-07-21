param(
    [ValidateSet(
        "gcc-debug",
        "gcc-debug-tests",
        "gcc-release-tests",
        "clang-debug-tests",
        "clang-release-tests"
    )]
    [string]$Preset = "gcc-debug"
)

$ErrorActionPreference = "Stop"

$buildDirByPreset = @{
    "gcc-debug" = "build/gcc-debug"
    "gcc-debug-tests" = "build/gcc-debug-tests"
    "gcc-release-tests" = "build/gcc-release-tests"
    "clang-debug-tests" = "build/clang-debug-tests"
    "clang-release-tests" = "build/clang-release-tests"
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
