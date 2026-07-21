param(
    [ValidateSet("gcc", "clang", "both")]
    [string]$Compiler = "gcc",
    [string]$Msys2Root = "C:\msys64",
    [switch]$SkipUpdate
)

$ErrorActionPreference = "Stop"
$bash = Join-Path $Msys2Root "usr\bin\bash.exe"

if (-not (Test-Path $bash -PathType Leaf)) {
    throw @"
MSYS2 was not found at '$Msys2Root'.
Install MSYS2 first, then rerun this script.
Default installer location: C:\msys64
"@
}

if (-not $SkipUpdate) {
    Write-Host "Updating MSYS2..."
    for ($pass = 1; $pass -le 2; $pass++) {
        & $bash -lc "pacman -Syu --noconfirm"
        if ($LASTEXITCODE -ne 0) {
            throw "MSYS2 update pass $pass failed with exit code $LASTEXITCODE."
        }
    }
}

$packages = @(
    "mingw-w64-ucrt-x86_64-toolchain",
    "mingw-w64-ucrt-x86_64-cmake",
    "mingw-w64-ucrt-x86_64-ninja"
)

if ($Compiler -in @("clang", "both")) {
    $packages += "mingw-w64-ucrt-x86_64-clang"
    $packages += "mingw-w64-ucrt-x86_64-lld"
}

$packageList = $packages -join " "
Write-Host "Installing MSYS2 UCRT64 build packages..."
& $bash -lc "pacman -S --needed --noconfirm $packageList"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Toolchain installed. Open 'MSYS2 UCRT64' and run:"
Write-Host "  cmake --preset gcc-debug-tests"
Write-Host "  cmake --build --preset build-gcc-debug-tests"
Write-Host "  ctest --preset test-gcc-debug"
