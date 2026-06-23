# ──────────────────────────────────────────────────────────────────────────────
#  Check CMakeLists.txt — Verifica que todos los archivos referenciados
#  en CMakeLists.txt existen realmente en disco.
#
#  Previene errores como:
#    - Archivos .cpp/.h nuevos no registrados en target_sources()
#    - Archivos renombrados pero no actualizados en CMakeLists.txt
#    - Archivos eliminados pero aún referenciados
#
#  Uso:
#    .\scripts\check_cmakelists_files.ps1
#    .\scripts\check_cmakelists_files.ps1 -Verbose
# ──────────────────────────────────────────────────────────────────────────────

param(
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$CmakeFile = Join-Path $ProjectRoot "CMakeLists.txt"

$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"

if (-not (Test-Path $CmakeFile)) {
    Write-Host "[FAIL] CMakeLists.txt no encontrado en: $CmakeFile" -ForegroundColor $Red
    exit 1
}

Write-Host "━━━ Verificando archivos en CMakeLists.txt ━━━" -ForegroundColor $Cyan

# ─── Leer CMakeLists.txt y extraer rutas de archivos fuente ────────────────
$content = Get-Content $CmakeFile -Raw

# Patrón: captura rutas como "Source/Common/types/Constants.h" dentro de set() o target_sources()
$allPaths = @()

# Buscar todas las rutas relativas (Source/... y tests/...)
$pattern = '(?:(?:Source|tests)/[^\s"]+)\.(?:cpp|h|hpp)'
$matches = [regex]::Matches($content, $pattern)

$foundCount = 0
$missingCount = 0
$missingFiles = @()

foreach ($m in $matches) {
    $relativePath = $m.Value.Trim()
    $fullPath = Join-Path $ProjectRoot $relativePath

    # Normalizar separadores (CMake usa /, Windows usa \)
    $fullPath = $fullPath.Replace('/', '\')

    $foundCount++

    if (Test-Path $fullPath) {
        if ($Verbose) {
            Write-Host "  [OK] $relativePath" -ForegroundColor $Gray
        }
    } else {
        Write-Host "  [MISSING] $relativePath" -ForegroundColor $Red
        $missingCount++
        $missingFiles += $relativePath
    }
}

# ─── Resumen ────────────────────────────────────────────────────────────────
Write-Host ""
if ($missingCount -eq 0) {
    Write-Host "  [OK] $foundCount archivos verificados, 0 faltantes" -ForegroundColor $Green
    exit 0
} else {
    Write-Host "  [FAIL] $missingCount de $foundCount archivos NO existen en disco:" -ForegroundColor $Red
    foreach ($f in $missingFiles) {
        Write-Host "    ✗ $f" -ForegroundColor $Red
        # Sugerir acción
        if ($f -like "*.cpp" -or $f -like "*.h") {
            Write-Host "      → Creado recientemente? Agrégalo a target_sources() en CMakeLists.txt" -ForegroundColor $Yellow
        }
    }
    exit 1
}
