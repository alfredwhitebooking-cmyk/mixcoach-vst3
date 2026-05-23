# ──────────────────────────────────────────────────────────────────────────────
#  MixCoach — Deploy VST3 Manual
#  Ejecutar DESPUES de cerrar FL Studio (o cualquier DAW que use los plugins)
# ──────────────────────────────────────────────────────────────────────────────
# Uso: .\DeployVST3.ps1
# ──────────────────────────────────────────────────────────────────────────────

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$VST3_System = "C:\Program Files\Common Files\VST3"

Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      MixCoach — Despliegue Manual de VST3                  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Verificar que el build existe
$MixCoachVST3 = Join-Path $BuildDir "MixCoach_artefacts\Release\VST3\MixCoach.vst3"
$MessengerVST3 = Join-Path $BuildDir "Messenger_artefacts\Release\VST3\Messenger.vst3"

if (-not (Test-Path $MixCoachVST3)) {
    Write-Host "[ERROR] No se encuentra MixCoach.vst3 en:" -ForegroundColor Red
    Write-Host "        $MixCoachVST3" -ForegroundColor Red
    Write-Host "        Compila primero con: cmake --build build --config Release" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $MessengerVST3)) {
    Write-Host "[ERROR] No se encuentra Messenger.vst3 en:" -ForegroundColor Red
    Write-Host "        $MessengerVST3" -ForegroundColor Red
    Write-Host "        Compila primero con: cmake --build build --config Release" -ForegroundColor Yellow
    exit 1
}

Write-Host "[INFO] Build encontrado:" -ForegroundColor Green
Write-Host "       MixCoach:  $MixCoachVST3" -ForegroundColor Gray
Write-Host "       Messenger: $MessengerVST3" -ForegroundColor Gray
Write-Host ""

# Verificar que el directorio destino existe
if (-not (Test-Path $VST3_System)) {
    Write-Host "[INFO] Creando directorio: $VST3_System" -ForegroundColor Yellow
    New-Item $VST3_System -ItemType Directory -Force | Out-Null
}

# ─── Limpiar viejos duplicados ─────────────────────────────────────────────
Write-Host "[Cleanup] Eliminando versiones antiguas duplicadas..." -ForegroundColor Yellow
$oldDuplicates = @(
    "MixCoach_2.vst3", "MixCoach-2.vst3", "MixCoach-1.vst3",
    "Messenger_2.vst3", "Messenger-2.vst3", "Messenger-1.vst3"
)
foreach ($dup in $oldDuplicates) {
    $path = Join-Path $VST3_System $dup
    if (Test-Path $path) {
        Remove-Item $path -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "  [Eliminado] $dup" -ForegroundColor Gray
    }
}

# ─── Desplegar MixCoach ────────────────────────────────────────────────────
Write-Host ""
Write-Host "[MixCoach] Desplegando..." -ForegroundColor Cyan
$targetMix = Join-Path $VST3_System "MixCoach.vst3"
try {
    if (Test-Path $targetMix) {
        Remove-Item $targetMix -Recurse -Force
        Write-Host "[MixCoach] Version anterior eliminada" -ForegroundColor Gray
    }
    Copy-Item $MixCoachVST3 $targetMix -Recurse -Force
    Write-Host "[MixCoach] ✅ Desplegado exitosamente" -ForegroundColor Green
} catch {
    Write-Host "[MixCoach] ❌ ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "          Asegurate de que FL Studio esta CERRADO." -ForegroundColor Yellow
    exit 1
}

# ─── Desplegar Messenger ───────────────────────────────────────────────────
Write-Host "[Messenger] Desplegando..." -ForegroundColor Cyan
$targetMsg = Join-Path $VST3_System "Messenger.vst3"
try {
    if (Test-Path $targetMsg) {
        Remove-Item $targetMsg -Recurse -Force
        Write-Host "[Messenger] Version anterior eliminada" -ForegroundColor Gray
    }
    Copy-Item $MessengerVST3 $targetMsg -Recurse -Force
    Write-Host "[Messenger] ✅ Desplegado exitosamente" -ForegroundColor Green
} catch {
    Write-Host "[Messenger] ❌ ERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "           Asegurate de que FL Studio esta CERRADO." -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      ✅ Ambos VST3 desplegados correctamente                ║" -ForegroundColor Cyan
Write-Host "║                                                             ║" -ForegroundColor Cyan
Write-Host "║      Ya puedes abrir FL Studio y cargar los plugins.       ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
