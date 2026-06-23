#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Run MixCoach stress tests: TestStress128Slots + TestStress50Tracks.
.DESCRIPTION
    Builds and runs the two stress tests that verify maximum load handling.
    Exit code 0 = all tests pass, 1 = any failure.
.PARAMETER NoBuild
    Skip building, only run existing test executables.
.PARAMETER Config
    Build configuration: Release (default) or Debug.
#>

param(
    [switch]$NoBuild,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$StartTime = Get-Date

$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"

function Write-Step($msg) { Write-Host "`n━━━ $msg ━━━" -ForegroundColor $Cyan }
function Write-OK($msg)   { Write-Host "  [OK] $msg" -ForegroundColor $Green }
function Write-Warn($msg) { Write-Host "  [!!] $msg" -ForegroundColor $Yellow }
function Write-Err($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor $Red }

Write-Host @"
╔═══════════════════════════════════════════════════════════════╗
║     MixCoach — Stress Test Runner                           ║
║     $(Get-Date -Format 'yyyy-MM-dd HH:mm')                             ║
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor $Cyan

$Tests = @("TestStress50Tracks", "TestStress128Slots")
$PassCount = 0
$FailCount = 0

# ─── Step 1: Build (optional) ─────────────────────────────────────────────
if (-not $NoBuild) {
    Write-Step "STEP 1/2: Building stress test targets ($Config)..."
    foreach ($test in $Tests) {
        Write-Host "  Building $test..." -ForegroundColor $Gray
        $output = cmake --build $BuildDir --config $Config --target $test 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Err "$test build failed!"
            Write-Host ($output | Select-Object -Last 10)
            exit 1
        }
        Write-OK "$test built successfully"
    }
} else {
    Write-Step "STEP 1/2: Skipping build (-NoBuild)"
}

# ─── Step 2: Run tests ────────────────────────────────────────────────────
Write-Step "STEP 2/2: Running stress tests..."

foreach ($testName in $Tests) {
    # Find the executable
    $exePaths = @(
        (Join-Path $BuildDir "tests/$Config/$testName.exe"),
        (Join-Path $BuildDir "tests/$testName.exe"),
        (Join-Path $BuildDir "tests/Debug/$testName.exe")
    )
    $exePath = $null
    foreach ($p in $exePaths) {
        if (Test-Path $p) { $exePath = $p; break }
    }

    if (-not $exePath) {
        Write-Warn "$testName.exe not found in build/tests/"
        $FailCount++
        continue
    }

    Write-Host "  Running $testName..." -ForegroundColor $Gray

    try {
        $output = & $exePath 2>&1
        $exitCode = $LASTEXITCODE

        # Parse results
        $passMatch = ($output | Select-String -Pattern "(\d+) passed")
        $failMatch = ($output | Select-String -Pattern "(\d+) failed")
        $passCountTest = if ($passMatch) { $passMatch.Matches.Groups[1].Value } else { "?" }
        $failCountTest = if ($failMatch) { $failMatch.Matches.Groups[1].Value } else { "?" }

        if ($exitCode -eq 0) {
            Write-OK "$testName : $passCountTest passed, $failCountTest failed"
            $PassCount++
        } else {
            Write-Err "$testName : $passCountTest passed, $failCountTest failed"
            # Show FAIL lines
            $output | Select-String -Pattern "FAIL|❌" | ForEach-Object {
                Write-Host "    $_" -ForegroundColor $Red
            }
            $FailCount++
        }
    }
    catch {
        Write-Err "$testName crashed: $_"
        $FailCount++
    }
}

# ─── Summary ──────────────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
Write-Host @"

═══════════════════════════════════════════════════════════════
  STRESS TEST RESULTS
  Duration: $($Duration.TotalSeconds.ToString('0.0'))s
  Pass: $PassCount  |  Fail: $FailCount
═══════════════════════════════════════════════════════════════
"@ -ForegroundColor $(if ($FailCount -gt 0) { $Red } else { $Green })

if ($FailCount -gt 0) {
    Write-Host "`n❌ STRESS TESTS FAILED" -ForegroundColor $Red
    exit 1
} else {
    Write-Host "`n✅ ALL STRESS TESTS PASSED" -ForegroundColor $Green
    exit 0
}
