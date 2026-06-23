#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Run AI validation tools (pattern_validator + ai_build_loop) from PowerShell.
.DESCRIPTION
    Wraps the Python-based AI tools for seamless integration into build.ps1,
    build_fast.ps1, validate.ps1, and CI pipelines.
    
    All tools return 0 on success, 1 on failure.
.PARAMETER Tool
    Which tool to run: "pattern-validator", "ai-build-loop", or "all".
.PARAMETER Args
    Additional arguments to pass to the tool.
.PARAMETER BuildLog
    Path to a build log file for ai_build_loop --analyze-only mode.
.PARAMETER Config
    Build configuration for ai_build_loop (Release/Debug).
.EXAMPLE
    .\scripts\run_ai_tools.ps1 -Tool pattern-validator
    .\scripts\run_ai_tools.ps1 -Tool ai-build-loop -BuildLog build_output.txt
    .\scripts\run_ai_tools.ps1 -Tool ai-build-loop -Config Debug
#>

param(
    [ValidateSet("pattern-validator", "ai-build-loop", "all")]
    [string]$Tool = "all",

    [string]$BuildLog = "",

    [string]$Config = "Release",

    [switch]$Help
)

$ErrorActionPreference = "Continue"  # Don't stop on Python warnings
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$StartTime = Get-Date

$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"

function Write-Step($msg)  { Write-Host "`n━━━ $msg ━━━" -ForegroundColor $Cyan }
function Write-OK($msg)    { Write-Host "  [OK] $msg" -ForegroundColor $Green }
function Write-Warn($msg)  { Write-Host "  [!!] $msg" -ForegroundColor $Yellow }
function Write-Err($msg)   { Write-Host "  [FAIL] $msg" -ForegroundColor $Red }

if ($Help) {
    Write-Host @"
MixCoach AI Tools Runner v1.0

USO:
  .\scripts\run_ai_tools.ps1 -Tool pattern-validator
    → Valida el código fuente contra reglas arquitectónicas
    → Se ejecuta PRE-build (lint arquitectónico)

  .\scripts\run_ai_tools.ps1 -Tool ai-build-loop -BuildLog build_output.txt
    → Analiza un log de build en busca de errores conocidos
    → Match con ERROR_PATTERNS.json + sugerencias de fix
    → Se ejecuta POST-build (solo si falló)

  .\scripts\run_ai_tools.ps1 -Tool ai-build-loop -Config Debug
    → Build completo + análisis de errores con auto-retry

  .\scripts\run_ai_tools.ps1 -Tool all
    → Ejecuta pattern-validator + ai-build-loop --stats

EXIT CODES:
  0 = Todo OK
  1 = Algún check falló
"@
    exit 0
}

$exitCode = 0

# ─── Pattern Validator ─────────────────────────────────────────────────────
if ($Tool -eq "pattern-validator" -or $Tool -eq "all") {
    Write-Step "PATTERN VALIDATOR — Verificando reglas arquitectónicas"
    Write-Host "  Revisando: PX1-PX7 (prohibidos), P1/P6/P8 (permitidos), D1/D2 (desaconsejados)" -ForegroundColor $Gray

    $pvScript = Join-Path $ProjectRoot "scripts/pattern_validator.py"
    if (-not (Test-Path $pvScript)) {
        Write-Warn "pattern_validator.py no encontrado en $pvScript"
    } else {
        $pvOutput = python $pvScript 2>&1
        $pvExit = $LASTEXITCODE

        if ($pvExit -eq 0) {
            Write-OK "Pattern validator: todos los checks pasaron"
        } else {
            Write-Err "Pattern validator: ALGUNOS CHECKS FALLARON"
            $exitCode = 1
        }

        # Mostrar output (omitir líneas vacías)
        $pvOutput | ForEach-Object {
            if ($_.Trim() -ne "") { Write-Host "  $_" }
        }
    }
}

# ─── AI Build Loop ─────────────────────────────────────────────────────────
if ($Tool -eq "ai-build-loop" -or $Tool -eq "all") {
    $blScript = Join-Path $ProjectRoot "ai_build_loop.py"

    if (-not (Test-Path $blScript)) {
        Write-Warn "ai_build_loop.py no encontrado en $blScript"
    } elseif ($BuildLog -ne "") {
        # Modo analyze-only: analiza un log existente
        Write-Step "AI BUILD LOOP — Analizando log de build"
        Write-Host "  Log: $BuildLog" -ForegroundColor $Gray

        $blOutput = python $blScript --analyze-only $BuildLog 2>&1
        $blExit = $LASTEXITCODE
        $blOutput | ForEach-Object {
            if ($_.Trim() -ne "") { Write-Host "  $_" }
        }

        if ($blExit -ne 0) {
            Write-Warn "ai_build_loop encontró errores en el log"
        } else {
            Write-OK "ai_build_loop: sin errores conocidos en el log"
        }
    } elseif ($Tool -eq "ai-build-loop") {
        # Modo build completo: ejecuta cmake + análisis + auto-retry
        Write-Step "AI BUILD LOOP — Build + análisis con auto-retry"
        Write-Host "  Config: $Config" -ForegroundColor $Gray

        $blArgs = @("--config", $Config, "--no-retry")
        $blOutput = python $blScript @blArgs 2>&1
        $blExit = $LASTEXITCODE

        $blOutput | ForEach-Object {
            if ($_.Trim() -ne "") { Write-Host "  $_" }
        }

        if ($blExit -ne 0) {
            Write-Err "ai_build_loop: BUILD FALLÓ"
            $exitCode = 1
        } else {
            Write-OK "ai_build_loop: build + análisis completado"
        }
    } elseif ($Tool -eq "all") {
        # Modo stats (parte de 'all'): muestra estadísticas de patrones de error
        Write-Step "AI BUILD LOOP — Estadísticas de errores"
        $blOutput = python $blScript --stats 2>&1
        $blOutput | ForEach-Object {
            if ($_.Trim() -ne "") { Write-Host "  $_" }
        }
    }
}

# ─── Resumen ───────────────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
Write-Host ""
if ($exitCode -eq 0) {
    Write-OK "AI tools check completado en $($Duration.TotalSeconds.ToString('0.0'))s"
} else {
    Write-Err "AI tools check COMPLETADO CON ERRORES en $($Duration.TotalSeconds.ToString('0.0'))s"
}

exit $exitCode
