# ──────────────────────────────────────────────────────────────────────────────
#  MixCoach — Build + Deploy Automático
#  Un solo comando para compilar y desplegar ambos plugins VST3
# ──────────────────────────────────────────────────────────────────────────────
# Uso:
#   .\build.ps1            # Compila Release + deploy automático
#   .\build.ps1 -Clean     # Limpia build anterior y recompila
#   .\build.ps1 -Debug     # Compila Debug (para desarrollo)
#   .\build.ps1 -NoDeploy  # Solo compila, no despliega
# ──────────────────────────────────────────────────────────────────────────────

param(
    [switch]$Clean,
    [switch]$Debug,
    [switch]$NoDeploy,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$Config = if ($Debug) { "Debug" } else { "Release" }
$StartTime = Get-Date

# ─── Colores ──────────────────────────────────────────────────────────────────
$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"
$Magenta = "Magenta"

function Write-Step($msg) { Write-Host "`n━━━ $msg ━━━" -ForegroundColor $Cyan }
function Write-OK($msg)   { Write-Host "  ✅ $msg" -ForegroundColor $Green }
function Write-Warn($msg) { Write-Host "  ⚠️  $msg" -ForegroundColor $Yellow }
function Write-Err($msg)  { Write-Host "  ❌ $msg" -ForegroundColor $Red }

# ─── Help ─────────────────────────────────────────────────────────────────────
if ($Help) {
    Write-Host @"
╔═══════════════════════════════════════════════════════════════╗
║     MixCoach — Build + Deploy Automático                    ║
╚═══════════════════════════════════════════════════════════════╝

USO:
  .\build.ps1             Compila Release + deploy
  .\build.ps1 -Clean      Limpia y recompila
  .\build.ps1 -Debug      Compila Debug
  .\build.ps1 -NoDeploy   Solo compila sin deploy

EJEMPLOS:
  .\build.ps1                         # Build rapido
  .\build.ps1 -Clean                  # Build desde cero
  .\build.ps1 -Debug -NoDeploy        # Debug sin deploy
"@
    exit 0
}

Write-Host @"
╔═══════════════════════════════════════════════════════════════╗
║     MixCoach — Build System v1.0                            ║
║     $(Get-Date -Format 'yyyy-MM-dd HH:mm')                          ║
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor $Cyan

# ─── 1. Configurar CMake si es necesario ──────────────────────────────────────
Write-Step "PASO 1/4: Verificando configuracion de CMake"

$cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
$needsConfig = $Clean -or -not (Test-Path $cmakeCache)

if ($needsConfig) {
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Warn "Limpiando build anterior..."
        Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-OK "Build anterior eliminado"
    }

    Write-Host "  Configurando CMake en $BuildDir..." -ForegroundColor $Gray
    $configResult = cmake -B $BuildDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Error de configuracion CMake:"
        Write-Host $configResult -ForegroundColor $Red
        exit 1
    }
    Write-OK "CMake configurado correctamente"
} else {
    Write-OK "CMake ya configurado (usa -Clean para reconfigurar)"
}

# ─── 2. Compilar ──────────────────────────────────────────────────────────────
Write-Step "PASO 2/4: Compilando ($Config)"

$buildLog = Join-Path $ProjectRoot "build_output.txt"
$buildResult = cmake --build $BuildDir --config $Config 2>&1 | Tee-Object -FilePath $buildLog

if ($LASTEXITCODE -ne 0) {
    Write-Err "ERROR DE COMPILACION"
    Write-Host "  Revisa el log completo en: build_output.txt" -ForegroundColor $Yellow
    Write-Host "  Ultimas 20 lineas del error:" -ForegroundColor $Yellow
    $buildResult | Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" -ForegroundColor $Red }
    exit 1
}

Write-OK "Compilacion exitosa"

# ─── 3. Verificar artefactos ──────────────────────────────────────────────────
Write-Step "PASO 3/4: Verificando artefactos generados"

$mixCoachVST3 = Join-Path $BuildDir "MixCoach_artefacts\$Config\VST3\MixCoach.vst3"
$messengerVST3 = Join-Path $BuildDir "Messenger_artefacts\$Config\VST3\Messenger.vst3"
$mixCoachExe = Join-Path $BuildDir "MixCoach_artefacts\$Config\Standalone\MixCoach.exe"
$messengerExe = Join-Path $BuildDir "Messenger_artefacts\$Config\Standalone\Messenger.exe"

$allOk = $true
if (Test-Path $mixCoachVST3) { Write-OK "MixCoach.vst3 generado" } else { Write-Warn "MixCoach.vst3 NO encontrado (puede ser Standalone-only)" }
if (Test-Path $messengerVST3) { Write-OK "Messenger.vst3 generado" } else { Write-Warn "Messenger.vst3 NO encontrado (puede ser Standalone-only)" }
if (Test-Path $mixCoachExe) { Write-OK "MixCoach.exe (Standalone) generado" }
if (Test-Path $messengerExe) { Write-OK "Messenger.exe (Standalone) generado" }

# ─── 4. Desplegar VST3 ────────────────────────────────────────────────────────
if (-not $NoDeploy) {
    Write-Step "PASO 4/4: Desplegando VST3"

    $deployScript = Join-Path $ProjectRoot "DeployVST3.ps1"
    if (Test-Path $deployScript) {
        & $deployScript
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Deploy manual fallo - puedes copiar manualmente los .vst3"
        } else {
            Write-OK "Plugins desplegados en C:\\Program Files\\Common Files\\VST3\\"
        }
    } else {
        Write-Warn "DeployVST3.ps1 no encontrado - deploy manual required"
    }
} else {
    Write-Step "PASO 4/4: Omitido (-NoDeploy)"
    Write-OK "Los VST3 estan en:"
    Write-Host "  MixCoach:  $mixCoachVST3" -ForegroundColor $Gray
    Write-Host "  Messenger: $messengerVST3" -ForegroundColor $Gray
}

# ─── Resumen Final ────────────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
Write-Host @"

╔═══════════════════════════════════════════════════════════════╗
║     ✅ BUILD COMPLETADO en $($Duration.TotalSeconds.ToString('F1'))s
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor $Green

Write-Host "  Configuracion: $Config"
if (-not $NoDeploy) {
    Write-Host "  Deploy:        Automático en C:\Program Files\Common Files\VST3\" -ForegroundColor $Gray
}
Write-Host "  Log:           build_output.txt" -ForegroundColor $Gray
Write-Host "`n  Para probar en FL Studio:" -ForegroundColor $Yellow
Write-Host "    1. Abre FL Studio" -ForegroundColor $Yellow
Write-Host "    2. Carga MixCoach en el Master" -ForegroundColor $Yellow
Write-Host "    3. Carga Messenger en cada pista" -ForegroundColor $Yellow
Write-Host "    4. Si no aparecen: Opciones -> Gestionar plugins -> Escaneo rapido" -ForegroundColor $Yellow
