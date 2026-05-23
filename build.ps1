# ──────────────────────────────────────────────────────────────────────────────
#  MixCoach — Build + Deploy Automático v1.1
#  Un solo comando para compilar y desplegar ambos plugins VST3
# ──────────────────────────────────────────────────────────────────────────────
# Uso:
#   .\build.ps1               Compila Release + deploy automático
#   .\build.ps1 -Clean        Limpia build anterior y recompila
#   .\build.ps1 -Debug        Compila Debug (para desarrollo)
#   .\build.ps1 -NoDeploy     Solo compila, no despliega
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

function Write-Step($msg) { Write-Host "`n━━━ $msg ━━━" -ForegroundColor $Cyan }
function Write-OK($msg)   { Write-Host "  [OK] $msg" -ForegroundColor $Green }
function Write-Warn($msg) { Write-Host "  [!!] $msg" -ForegroundColor $Yellow }
function Write-Err($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor $Red }

# ─── Help ─────────────────────────────────────────────────────────────────────
if ($Help) {
    Write-Host @"
╔═══════════════════════════════════════════════════════════════╗
║     MixCoach — Build + Deploy Automatico                    ║
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
║     MixCoach - Build System v1.1                            ║
║     $(Get-Date -Format 'yyyy-MM-dd HH:mm')                            ║
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor $Cyan

Write-Host "  Modo: $Config | Clean: $Clean | Deploy: $(-not $NoDeploy)" -ForegroundColor $Gray

# ─── 0. Verificar git status ───────────────────────────────────────────────
Write-Step "PASO 0/5: Verificando estado del repositorio"
$gitStatus = git -C $ProjectRoot status --porcelain 2>&1
if ($gitStatus -and $gitStatus -isnot [System.ComponentModel.Win32Exception]) {
    Write-Warn "Hay cambios sin commit:"
    $gitStatus | ForEach-Object { Write-Host "    $_" -ForegroundColor $Yellow }
    Write-Host "  Considera hacer: git add -A && git commit -m \"mensaje\"" -ForegroundColor $Yellow
    Write-Host "  (continuando con el build...)" -ForegroundColor $Gray
} else {
    Write-OK "Repositorio limpio"
}

# ─── 1. Verificar JUCE ──────────────────────────────────────────────────────
Write-Step "PASO 1/5: Verificando JUCE"

# Leer ruta de JUCE desde CMakeLists.txt (fuente única de verdad)
$juceRoot = Select-String -Path "$ProjectRoot/CMakeLists.txt" -Pattern 'set\(JUCE_ROOT "(.+)"\)' | ForEach-Object { $_.Matches.Groups[1].Value }

if (-not $juceRoot) {
    $juceRoot = "C:/MIX COACH SYSTEM/juce-8.0.13-windows/MixCoachAI/JUCE"
    Write-Warn "No se pudo leer JUCE_ROOT de CMakeLists.txt, usando default"
}

$juceOk = Test-Path "$juceRoot/CMakeLists.txt"
if ($juceOk) {
    Write-OK "JUCE 8 encontrado"
} else {
    Write-Err "JUCE no encontrado en: $juceRoot"
    Write-Host "  Revisa la ruta en CMakeLists.txt (JUCE_ROOT)" -ForegroundColor $Yellow
    exit 1
}

# ─── 2. Configurar CMake ──────────────────────────────────────────────────────
Write-Step "PASO 2/5: Configurando CMake"

$cmakeCache = Join-Path $BuildDir "CMakeCache.txt"
$needsConfig = $Clean -or -not (Test-Path $cmakeCache)

if ($needsConfig) {
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Warn "Limpiando build anterior..."
        Remove-Item $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
        if (Test-Path $BuildDir) {
            Write-Err "No se pudo limpiar build/ - archivos bloqueados?"
            Write-Host "  Cierra Visual Studio y programas que usen la build" -ForegroundColor $Yellow
            exit 1
        }
        Write-OK "Build anterior eliminado"
    }

    Write-Host "  Ejecutando: cmake -B $BuildDir ..." -ForegroundColor $Gray
    $configResult = cmake -B $BuildDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Error de configuracion CMake"
        Write-Host $configResult -ForegroundColor $Red
        exit 1
    }
    Write-OK "CMake configurado correctamente"
} else {
    Write-OK "CMake ya configurado (usa -Clean para reconfigurar)"
}

# ─── 3. Compilar ──────────────────────────────────────────────────────────────
Write-Step "PASO 3/5: Compilando ($Config)"

$buildLog = Join-Path $ProjectRoot "build_output.txt"
$buildResult = cmake --build $BuildDir --config $Config 2>&1 | Tee-Object -FilePath $buildLog

if ($LASTEXITCODE -ne 0) {
    Write-Err "ERROR DE COMPILACION"
    Write-Host "  Revisa el log: build_output.txt" -ForegroundColor $Yellow
    Write-Host "  Ultimas 20 lineas del error:" -ForegroundColor $Yellow
    $buildResult | Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" -ForegroundColor $Red }
    exit 1
}

Write-OK "Compilacion exitosa ($Config)"

# ─── 4. Verificar artefactos ──────────────────────────────────────────────────
Write-Step "PASO 4/5: Verificando artefactos"

$mixCoachVST3 = Join-Path $BuildDir "MixCoach_artefacts\$Config\VST3\MixCoach.vst3"
$messengerVST3 = Join-Path $BuildDir "Messenger_artefacts\$Config\VST3\Messenger.vst3"
$mixCoachExe = Join-Path $BuildDir "MixCoach_artefacts\$Config\Standalone\MixCoach.exe"
$messengerExe = Join-Path $BuildDir "Messenger_artefacts\$Config\Standalone\Messenger.exe"

if (Test-Path $mixCoachVST3)  { Write-OK "MixCoach.vst3 generado" }
if (Test-Path $messengerVST3) { Write-OK "Messenger.vst3 generado" }
if (Test-Path $mixCoachExe)   { Write-OK "MixCoach.exe (Standalone)" }
if (Test-Path $messengerExe)  { Write-OK "Messenger.exe (Standalone)" }

# ─── 5. Desplegar VST3 ────────────────────────────────────────────────────────
if (-not $NoDeploy) {
    Write-Step "PASO 5/5: Desplegando VST3"

    $deployScript = Join-Path $ProjectRoot "DeployVST3.ps1"
    if (Test-Path $deployScript) {
        powershell -ExecutionPolicy Bypass -File $deployScript
        if ($LASTEXITCODE -ne 0) {
            Write-Warn "Deploy fallo - FL Studio abierto?"
            Write-Host "  Cierra FL Studio y ejecuta: .\DeployVST3.ps1" -ForegroundColor $Yellow
        } else {
            Write-OK "Plugins en C:\Program Files\Common Files\VST3\"
        }
    } else {
        Write-Warn "DeployVST3.ps1 no encontrado - copia manual"
    }
} else {
    Write-Step "PASO 5/5: Omitido (-NoDeploy)"
    Write-OK "VST3 disponibles en:"
    Write-Host "  MixCoach:  $mixCoachVST3" -ForegroundColor $Gray
    Write-Host "  Messenger: $messengerVST3" -ForegroundColor $Gray
}

# ─── Resumen Final ────────────────────────────────────────────────────────────
$Duration = (Get-Date) - $StartTime
Write-Host @"

╔═══════════════════════════════════════════════════════════════╗
║   BUILD COMPLETADO en $($Duration.TotalSeconds.ToString('F1'))s
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor $Green

Write-Host "  Modo: $Config"
if (-not $NoDeploy) {
    Write-Host "  Deploy: Automatico" -ForegroundColor $Gray
}
Write-Host "  Log: build_output.txt" -ForegroundColor $Gray

Write-Host "`n  Para probar en FL Studio:" -ForegroundColor $Yellow
Write-Host "  1. Abre FL Studio" -ForegroundColor $Yellow
Write-Host "  2. Carga MixCoach en el Master" -ForegroundColor $Yellow
Write-Host "  3. Carga Messenger en cada pista" -ForegroundColor $Yellow
Write-Host "  4. Si no aparecen: Opciones -> Gestionar plugins -> Escaneo rapido" -ForegroundColor $Yellow
