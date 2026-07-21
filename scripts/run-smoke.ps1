param(
    [string]$BuildDir = "build/debug",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [ValidateRange(1, 300)]
    [int]$DurationSeconds = 5
)

$ErrorActionPreference = "Stop"

function Resolve-Executable {
    param(
        [string]$Name,
        [string]$Path
    )

    if (-not (Test-Path $Path -PathType Leaf)) {
        throw "Could not locate '$Name' at '$Path'. Build the selected preset and configuration first."
    }

    return (Resolve-Path $Path).Path
}

function Invoke-SmokeRun {
    param(
        [string]$Name,
        [string]$ExecutablePath,
        [int]$TimeoutSeconds
    )

    Write-Host "[INFO] Launching ${Name}: $ExecutablePath"
    $process = Start-Process -FilePath $ExecutablePath -PassThru -WorkingDirectory (Resolve-Path ".").Path
    Start-Sleep -Seconds $TimeoutSeconds

    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        Write-Host "[PASS] $Name remained running for $TimeoutSeconds seconds."
        return $true
    }

    if ($process.ExitCode -eq 0) {
        Write-Host "[PASS] $Name exited with code 0 before the timeout."
        return $true
    }

    Write-Host "[FAIL] $Name exited early with code $($process.ExitCode)."
    return $false
}

$runtimeDir = Join-Path $BuildDir "bin/$Config"
$basicGame = Resolve-Executable -Name "BasicGame" -Path (Join-Path $runtimeDir "BasicGame.exe")
$basicRendering = Resolve-Executable -Name "BasicRenderingExample" -Path (Join-Path $runtimeDir "BasicRenderingExample.exe")

$basicGamePassed = Invoke-SmokeRun -Name "BasicGame" -ExecutablePath $basicGame -TimeoutSeconds $DurationSeconds
$basicRenderingPassed = Invoke-SmokeRun -Name "BasicRenderingExample" -ExecutablePath $basicRendering -TimeoutSeconds $DurationSeconds

if (-not ($basicGamePassed -and $basicRenderingPassed)) {
    Write-Host "[FAIL] Smoke test failed."
    exit 1
}

Write-Host "[PASS] Smoke test completed successfully."
