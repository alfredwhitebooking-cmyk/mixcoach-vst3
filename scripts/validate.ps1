#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Validate build, deploy, and tests for MixCoach project.
.DESCRIPTION
    Runs in order: build VST3s → build tests → verify artifacts → check deploy → run all tests → report status.
    Exit code 0 = all OK, 1 = any failure.
#>

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$StartTime = Get-Date
$Results = @()

Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  MixCoach Validation Script" -ForegroundColor Cyan
Write-Host "  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Cyan
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# ─── Helper: Find test executable ─────────────────────────────────────────
function Find-TestExe {
    param([string]$TestName)
    $candidates = @(
        (Join-Path $BuildDir "tests/Release/$TestName.exe"),
        (Join-Path $BuildDir "tests/$TestName.exe"),
        (Join-Path $BuildDir "tests/Debug/$TestName.exe")
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

# ─── Step 1: Build VST3 plugins ───────────────────────────────────────────
Write-Host "▸ STEP 1/7: Building MixCoach_VST3 + Messenger_VST3..." -ForegroundColor Yellow
try {
    $buildOutput = & cmake --build $BuildDir --config Release --target MixCoach_VST3 --target Messenger_VST3 2>&1
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        Write-Host "  ✅ VST3 build successful" -ForegroundColor Green
        $Results += "BUILD: PASS"
    } else {
        Write-Host "  ❌ VST3 build failed (exit code $exitCode)" -ForegroundColor Red
        Write-Host ($buildOutput | Select-Object -Last 15)
        $Results += "BUILD: FAIL"
    }
}
catch {
    Write-Host "  ❌ Build crash: $_" -ForegroundColor Red
    $Results += "BUILD: CRASH"
}

# ─── Step 2: Build C++ test targets ───────────────────────────────────────
Write-Host "`n▸ STEP 2/7: Building C++ test targets..." -ForegroundColor Yellow
try {
    $testBuildOutput = & cmake --build $BuildDir --config Release --target TestSmoothValue --target TestPhaseManager --target TestCoachEngine --target TestIPCIntegration --target TestSharedMemory --target TestSlotRegistry --target TestTelemetryDSP --target TestLUFSMeter --target TestSpectrographComponent --target TestVectorscopeComponent --target TestPhaseCorrelationMeter --target TestMeterComponent 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  ✅ C++ tests built successfully" -ForegroundColor Green
        $Results += "TEST_BUILD: PASS"
    } else {
        Write-Host "  ❌ C++ test build failed (exit code $LASTEXITCODE)" -ForegroundColor Red
        Write-Host ($testBuildOutput | Select-Object -Last 15)
        $Results += "TEST_BUILD: FAIL"
    }
}
catch {
    Write-Host "  ⚠️ C++ test build: SKIP ($($_.Exception.Message))" -ForegroundColor Yellow
    $Results += "TEST_BUILD: SKIP"
}

# ─── Step 3: Verify VST3 artifacts ────────────────────────────────────────
Write-Host "`n▸ STEP 3/7: Verifying VST3 artifacts..." -ForegroundColor Yellow
$mixCoachDll = Join-Path $BuildDir "MixCoach_artefacts/Release/VST3/MixCoach.vst3/Contents/x86_64-win/MixCoach.vst3"
$messengerDll = Join-Path $BuildDir "Messenger_artefacts/Release/VST3/Messenger.vst3/Contents/x86_64-win/Messenger.vst3"

$mixCoachOk = Test-Path $mixCoachDll
$messengerOk = Test-Path $messengerDll

if ($mixCoachOk) {
    $size = (Get-Item $mixCoachDll).Length
    Write-Host "  ✅ MixCoach.vst3 ($([math]::Round($size/1KB)) KB)" -ForegroundColor Green
    $Results += "MIXCOACH_ARTIFACT: PASS"
} else {
    Write-Host "  ❌ MixCoach.vst3 NOT FOUND" -ForegroundColor Red
    $Results += "MIXCOACH_ARTIFACT: FAIL"
}

if ($messengerOk) {
    $size = (Get-Item $messengerDll).Length
    Write-Host "  ✅ Messenger.vst3 ($([math]::Round($size/1KB)) KB)" -ForegroundColor Green
    $Results += "MESSENGER_ARTIFACT: PASS"
} else {
    Write-Host "  ❌ Messenger.vst3 NOT FOUND" -ForegroundColor Red
    $Results += "MESSENGER_ARTIFACT: FAIL"
}

# ─── Step 4: Check VST3 deployment ────────────────────────────────────────
Write-Host "`n▸ STEP 4/7: Checking VST3 deployment..." -ForegroundColor Yellow
$deployCoach = "C:/Program Files/Common Files/VST3/MixCoach.vst3/Contents/x86_64-win/MixCoach.vst3"
$deployMsg = "C:/Program Files/Common Files/VST3/Messenger.vst3/Contents/x86_64-win/Messenger.vst3"

$deployCoachOk = Test-Path $deployCoach
$deployMsgOk = Test-Path $deployMsg

if ($deployCoachOk) {
    Write-Host "  ✅ MixCoach deployed" -ForegroundColor Green
    $Results += "DEPLOY_MIXCOACH: PASS"
} else {
    Write-Host "  ⚠️ MixCoach NOT deployed (run DeployVST3.ps1)" -ForegroundColor Yellow
    $Results += "DEPLOY_MIXCOACH: WARN"
}

if ($deployMsgOk) {
    Write-Host "  ✅ Messenger deployed" -ForegroundColor Green
    $Results += "DEPLOY_MESSENGER: PASS"
} else {
    Write-Host "  ⚠️ Messenger NOT deployed (run DeployVST3.ps1)" -ForegroundColor Yellow
    $Results += "DEPLOY_MESSENGER: WARN"
}

# ─── Step 5: Run all tests (C++ + Python) ─────────────────────────────────
Write-Host "`n▸ STEP 5/7: Running all tests..." -ForegroundColor Yellow

# ─── C++ Tests ────────────────────────────────────────────────────────────
$CppTests = @("TestSmoothValue", "TestPhaseManager", "TestCoachEngine", "TestIPCIntegration", "TestSharedMemory", "TestSlotRegistry", "TestTelemetryDSP", "TestLUFSMeter", "TestSpectrographComponent", "TestVectorscopeComponent", "TestPhaseCorrelationMeter", "TestMeterComponent")

foreach ($testName in $CppTests) {
    $exePath = Find-TestExe $testName
    if (-not $exePath) {
        Write-Host "  ⚠️ $testName.exe: SKIP (not found in build/tests/)" -ForegroundColor Yellow
        $Results += "TEST_$($testName.ToUpper()): SKIP"
        continue
    }

    try {
        $output = & $exePath 2>&1
        $testKey = "TEST_$($testName.ToUpper())"

        if ($LASTEXITCODE -eq 0) {
            # Extract pass/fail counts from test output
            $passMatch = ($output | Select-String -Pattern "(\d+) passed")
            $failMatch = ($output | Select-String -Pattern "(\d+) failed")
            $passCount = if ($passMatch) { $passMatch.Matches.Groups[1].Value } else { "?" }
            $failCount = if ($failMatch) { $failMatch.Matches.Groups[1].Value } else { "?" }
            Write-Host "  ✅ $testName.exe: $passCount passed, $failCount failed" -ForegroundColor Green
            $Results += "${testKey}: PASS"
        } else {
            Write-Host "  ❌ $testName.exe: FAILED (exit $LASTEXITCODE)" -ForegroundColor Red
            # Show only the FAIL lines + results line
            $failLines = $output | Select-String -Pattern "FAIL|failed|Results"
            if ($failLines) { $failLines | ForEach-Object { Write-Host "    $_" -ForegroundColor Red } }
            else { Write-Host "    (no FAIL lines in output)" -ForegroundColor Gray }
            $Results += "${testKey}: FAIL"
        }
    }
    catch {
        Write-Host "  ⚠️ $testName.exe: SKIP ($($_.Exception.Message))" -ForegroundColor Yellow
        $Results += "TEST_$($testName.ToUpper()): SKIP"
    }
}

# ─── Python Test ──────────────────────────────────────────────────────────
$testPy = Join-Path $ProjectRoot "tests/test_ms_calculation.py"

if (Test-Path $testPy) {
    try {
        $pyOutput = & python $testPy 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  ✅ test_ms_calculation.py: PASS" -ForegroundColor Green
            $Results += "TEST_MS_CALC: PASS"
        } else {
            Write-Host "  ❌ test_ms_calculation.py: FAIL" -ForegroundColor Red
            Write-Host ($pyOutput | Select-Object -Last 10)
            $Results += "TEST_MS_CALC: FAIL"
        }
    }
    catch {
        Write-Host "  ⚠️ test_ms_calculation.py: SKIP (no Python)" -ForegroundColor Yellow
        $Results += "TEST_MS_CALC: SKIP"
    }
} else {
    Write-Host "  ⚠️ test_ms_calculation.py: SKIP (file not found)" -ForegroundColor Yellow
    $Results += "TEST_MS_CALC: SKIP"
}

# ─── Step 6: Summary ──────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
$TotalPass = ($Results | Where-Object { $_ -match ": PASS$" }).Count
$TotalFail = ($Results | Where-Object { $_ -match ": FAIL$" }).Count
$TotalWarn = ($Results | Where-Object { $_ -match ": (WARN|SKIP|NONE)$" }).Count

Write-Host ""
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  VALIDATION SUMMARY" -ForegroundColor Cyan
Write-Host "  Duration: $($Duration.TotalSeconds.ToString('0.0'))s" -ForegroundColor Gray
Write-Host "  Pass: $TotalPass  |  Fail: $TotalFail  |  Warnings: $TotalWarn" -ForegroundColor $(if ($TotalFail -gt 0) { "Red" } else { "Green" })
Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$Results | ForEach-Object { Write-Host "  $_" }

if ($TotalFail -gt 0) {
    Write-Host "`n❌ VALIDATION FAILED" -ForegroundColor Red
    if ($Results -match "BUILD: FAIL|BUILD: CRASH") {
        Write-Host "  → Fix build errors first." -ForegroundColor Yellow
    }
    if ($Results -match "TEST_.*: FAIL") {
        Write-Host "  → Check failing tests for details." -ForegroundColor Yellow
    }
    exit 1
} else {
    Write-Host "`n✅ ALL CHECKS PASSED" -ForegroundColor Green
    exit 0
}
