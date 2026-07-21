param(
    [ValidateSet("gcc", "clang")]
    [string]$Compiler = "gcc",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$WithoutTests,
    [string]$Msys2Root = "C:\msys64"
)

$ErrorActionPreference = "Stop"
$toolBin = Join-Path $Msys2Root "ucrt64\bin"
$cmake = Join-Path $toolBin "cmake.exe"
$ctest = Join-Path $toolBin "ctest.exe"

if (-not (Test-Path $cmake -PathType Leaf)) {
    throw "MSYS2 UCRT64 CMake was not found. Run scripts/bootstrap-msys2.ps1 first."
}

$env:Path = "$toolBin;$env:Path"

if ($Compiler -eq "gcc") {
    if ($Configuration -eq "Release") {
        $configurePreset = "gcc-release-tests"
        $buildPreset = "build-gcc-release-tests"
        $testPreset = "test-gcc-release"
    } elseif ($WithoutTests) {
        $configurePreset = "gcc-debug"
        $buildPreset = "build-gcc-debug"
        $testPreset = $null
    } else {
        $configurePreset = "gcc-debug-tests"
        $buildPreset = "build-gcc-debug-tests"
        $testPreset = "test-gcc-debug"
    }
} else {
    if ($Configuration -eq "Release") {
        $configurePreset = "clang-release-tests"
        $buildPreset = "build-clang-release-tests"
        $testPreset = "test-clang-release"
    } else {
        $configurePreset = "clang-debug-tests"
        $buildPreset = "build-clang-debug-tests"
        $testPreset = "test-clang-debug"
    }
}

& $cmake --preset $configurePreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build --preset $buildPreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($testPreset) {
    & $ctest --preset $testPreset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
