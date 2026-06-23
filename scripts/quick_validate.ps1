#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick Validate — build rápido + tests del área modificada (~15s)
.DESCRIPTION
    Alternativa rápida a validate.ps1 (que tarda ~2-3 min).
    Detecta automáticamente qué archivos cambiaron y ejecuta solo los tests relevantes.

    Uso:
      .\scripts\quick_validate.ps1                     # Detecta cambios automáticamente
      .\scripts\quick_validate.ps1 -Target MixCoach     # Build + tests MixCoach
      .\scripts\quick_validate.ps1 -Target Messenger   # Build + tests Messenger
      .\scripts\quick_validate.ps1 -Target All          # Full build + all tests
      .\scripts\quick_validate.ps1 -CommitCheck         # Pre-commit: solo build afectado + tests

.PARAMETER Target
    Área a validar: MixCoach, Messenger, Common, All, Auto (default)

.PARAMETER CommitCheck
    Modo pre-commit: solo build del target afectado, sin deploy check
#>

param(
    [ValidateSet("Auto", "MixCoach", "Messenger", "Common", "All")]
    [string]$Target = "Auto",
    [switch]$CommitCheck
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BuildDir = Join-Path $ProjectRoot "build"
$StartTime = Get-Date
$Pass = 0; $Fail = 0; $Skip = 0

# ─── Color helpers ──────────────────────────────────────────────────
$Cyan = "Cyan"; $Green = "Green"; $Yellow = "Yellow"; $Red = "Red"; $Gray = "Gray"

# ─── Auto-detect target from git changes ────────────────────────────
function Get-AffectedTarget {
    $changed = & git diff --name-only HEAD 2>$null
    if (-not $changed) { $changed = & git status --porcelain 2>$null }
    $all = ($changed -join " ") 

    if ($all -match "Source/MixCoach/") { return "MixCoach" }
    if ($all -match "Source/Messenger/") { return "Messenger" }
    if ($all -match "Source/Common/") { return "Common" }
    return "All"
}

if ($Target -eq "Auto") {
    $Target = Get-AffectedTarget
    Write-Host "  ℹ️  Auto-detect: $Target" -ForegroundColor $Gray
}

# ─── Test map: area → test targets ──────────────────────────────────
$TestMap = @{
    "MixCoach" = @(
        "TestCoachEngine", "TestPhaseManager", "TestSemanticComparator",
        "TestTrackFeedCore", "TestSmoothValue",
        "TestExperienceLevel", "TestAudioAnalyzer", "TestAudioDescriptors"
    )
    "Common"   = @(
        "TestSlotRegistry", "TestSharedMemory", "TestIPCIntegration",
        "TestStress128Slots", "TestStress50Tracks", "TestLUFSMeter"
    )
    "Messenger" = @()
    "All"      = @(
        "TestCoachEngine", "TestPhaseManager", "TestSemanticComparator",
        "TestTrackFeedCore", "TestSmoothValue",
        "TestExperienceLevel", "TestAudioAnalyzer", "TestAudioDescriptors",
        "TestSlotRegistry", "TestSharedMemory", "TestIPCIntegration",
        "TestStress128Slots", "TestStress50Tracks", "TestLUFSMeter",
        "TestSpectrographComponent", "TestVectorscopeComponent",
        "TestPhaseCorrelationMeter", "TestStereoWidthMeter",
        "TestReferenceAudioPlayer"
    )
}

$TestTargets = $TestMap[$Target]
$BuildTarget = if ($Target -eq "All") { "MixCoach_VST3" } else { "${Target}_VST3" }
if ($Target -eq "Common") { $BuildTarget = "MixCoach_VST3" }  # Common se prueba via MixCoach

# ─── Run Step ───────────────────────────────────────────────────────
function Run-Step {
    param([string]$Label, [scriptblock]$Block)
    Write-Host "  ⏳ $Label..." -ForegroundColor $Yellow
    try {
        $result = & $Block
        $exit = $LASTEXITCODE
        if ($exit -eq 0 -or $exit -eq $null) {
            Write-Host "  ✅ $Label" -ForegroundColor $Green
            $script:Pass++
        } else {
            Write-Host "  ❌ $Label (exit $exit)" -ForegroundColor $Red
            if ($result) { Write-Host ($result | Select-Object -Last 5) -ForegroundColor $Red }
            $script:Fail++
        }
    } catch {
        $errMsg = $_.Exception.Message
        if ($errMsg -eq 'Tests failed') {
            Write-Host "  ❌ $Label" -ForegroundColor $Red
            $script:Fail++
        } else {
            Write-Host "  ⚠️  $Label SKIP: $errMsg" -ForegroundColor $Yellow
            $script:Skip++
        }
    }
}

# ─── Header ─────────────────────────────────────────────────────────
Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor $Cyan
Write-Host "║     MixCoach Quick Validate" -ForegroundColor $Cyan
Write-Host "║     Target: $Target  |  Mode: $(if($CommitCheck){'Pre-commit'}else{'Full'})" -ForegroundColor $Cyan
Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor $Cyan
Write-Host ""

# ─── Step 1: Build target ──────────────────────────────────────────
Run-Step -Label "Build $BuildTarget" -Block {
    cmake --build $BuildDir --config Release --target $BuildTarget 2>&1
}

# ─── Step 2: Build test targets ────────────────────────────────────
if ($TestTargets.Count -gt 0) {
    Run-Step -Label "Build $($TestTargets.Count) test targets" -Block {
        $targetArgs = @()
        foreach ($t in $TestTargets) {
            $targetArgs += "--target", $t
        }
        cmake --build $BuildDir --config Release @targetArgs 2>&1
    }
}

# ─── Step 3: Run tests ─────────────────────────────────────────────
$TestExeDir = Join-Path $BuildDir "tests/Release"
foreach ($testName in $TestTargets) {
    $exe = Join-Path $TestExeDir "$testName.exe"
    if (-not (Test-Path $exe)) {
        $exe = Join-Path $BuildDir "tests/$testName.exe"
    }
    if (-not (Test-Path $exe)) {
        Write-Host "  ⚠️  $testName.exe not found (SKIP)" -ForegroundColor $Yellow
        $Skip++
        continue
    }
    Run-Step -Label "$testName" -Block {
        $output = & $exe 2>&1
        if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
        $output | Select-String "Results" | ForEach-Object { Write-Host "       $_" -ForegroundColor $Gray }
        $output
    }
}

# ─── Summary ───────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor $Cyan
Write-Host "║  ✅ Pass: $Pass  |  ❌ Fail: $Fail  |  ⚠️  Skip: $Skip" -ForegroundColor $(if($Fail -gt 0){$Red}else{$Green})
Write-Host "║  ⏱️  $( $Duration.TotalSeconds.ToString('0.0') )s" -ForegroundColor $Gray
Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor $Cyan
Write-Host ""

if ($Fail -gt 0) { exit 1 } else { exit 0 }
