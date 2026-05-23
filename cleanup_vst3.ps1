Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   LIMPIEZA TOTAL MixCoach / Messenger" -ForegroundColor Cyan
Write-Host "   Elimina TODAS las copias VST3 del sistema" -ForegroundColor Cyan
Write-Host "   y la cache de plugins de FL Studio" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# ─── 1. Buscar VST3 en ubicaciones estándar ──────────────────────────────────
$vstLocations = @(
    "$env:LOCALAPPDATA\Programs\Common\VST3",
    "${env:ProgramFiles}\Common Files\VST3",
    "${env:ProgramFiles(x86)}\Common Files\VST3",
    "$env:LOCALAPPDATA\VST3",
    "C:\ProgramData\VST3"
)

Write-Host "--- PASO 1: Buscando VST3 de Messenger/MixCoach ---" -ForegroundColor Yellow

$foundVST3 = @()
foreach ($loc in $vstLocations) {
    if (Test-Path $loc) {
        Get-ChildItem -Path $loc -ErrorAction SilentlyContinue | Where-Object { 
            $_.PSIsContainer -and ($_.Name -like '*Messenger*' -or $_.Name -like '*MixCoach*')
        } | ForEach-Object {
            Write-Host "  ENCONTRADO: $($_.FullName)" -ForegroundColor Red
            $foundVST3 += $_.FullName
        }
    }
}

# Buscar en subdirectorios adicionales (FL Studio paths, etc.)
$extraDirs = @(
    "$env:LOCALAPPDATA",
    "${env:ProgramFiles}\Image-Line",
    "${env:ProgramFiles(x86)}\Image-Line",
    "C:\Program Files\Image-Line"
)

foreach ($base in $extraDirs) {
    if (Test-Path $base) {
        Get-ChildItem -Path $base -Recurse -Directory -ErrorAction SilentlyContinue | 
            Where-Object { $_.Name -like '*Messenger*' -or $_.Name -like '*MixCoach*' } |
            ForEach-Object {
                Write-Host "  ENCONTRADO: $($_.FullName)" -ForegroundColor Red
                $foundVST3 += $_.FullName
            }
    }
}

# Eliminar VST3
if ($foundVST3.Count -eq 0) {
    Write-Host "  No se encontraron archivos VST3 de Messenger/MixCoach." -ForegroundColor Green
} else {
    Write-Host "`n--- ELIMINANDO $($foundVST3.Count) archivos VST3 ---" -ForegroundColor Magenta
    foreach ($f in $foundVST3) {
        try {
            Remove-Item -Path $f -Recurse -Force -ErrorAction Stop
            Write-Host "  ELIMINADO: $f" -ForegroundColor Green
        } catch {
            Write-Host "  ERROR: $f - $_" -ForegroundColor Red
        }
    }
}

# ─── 2. Limpiar cache de FL Studio ────────────────────────────────────────────
Write-Host "`n--- PASO 2: Buscando cache de plugins de FL Studio ---" -ForegroundColor Yellow

# FL Studio stores plugin database/presets here
$flPaths = @(
    "$env:USERPROFILE\Documents\Image-Line\FL Studio\Plugins",
    "$env:USERPROFILE\Documents\Image-Line\FL Studio\Persist",
    "$env:APPDATA\Image-Line\FL Studio",
    "$env:LOCALAPPDATA\Image-Line\FL Studio"
)

$flFilesDeleted = 0
foreach ($fl in $flPaths) {
    if (Test-Path $fl) {
        Write-Host "  Escaneando: $fl" -ForegroundColor DarkGray
        
        # Buscar archivos .fst (plugin state) de Messenger/MixCoach
        Get-ChildItem -Path $fl -Recurse -ErrorAction SilentlyContinue | 
            Where-Object { $_.Name -like '*Messenger*' -or $_.Name -like '*MixCoach*' } |
            ForEach-Object {
                try {
                    Remove-Item -Path $_.FullName -Force -ErrorAction Stop
                    Write-Host "  ELIMINADO cache FL: $($_.Name)" -ForegroundColor Green
                    $flFilesDeleted++
                } catch {
                    Write-Host "  ERROR al eliminar $($_.Name): $_" -ForegroundColor Red
                }
            }
    }
}

if ($flFilesDeleted -eq 0) {
    Write-Host "  No se encontraron archivos de cache de FL Studio para Messenger/MixCoach." -ForegroundColor Green
} else {
    Write-Host "  Eliminados $flFilesDeleted archivos de cache de FL Studio." -ForegroundColor Green
}

# ─── 3. Buscar en todo el sistema C:\ cualquier VST3 residual ──────────────────
Write-Host "`n--- PASO 3: Busqueda ampliada en C:\ ---" -ForegroundColor Yellow

$rootDirs = @("C:\Program Files", "C:\Program Files (x86)", "C:\ProgramData")
foreach ($root in $rootDirs) {
    if (Test-Path $root) {
        Get-ChildItem -Path $root -Recurse -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'Messenger|MixCoach' -and $_.Extension -eq '.vst3' } |
            ForEach-Object {
                try {
                    Remove-Item -Path $_.FullName -Recurse -Force -ErrorAction Stop
                    Write-Host "  ELIMINADO residual: $($_.FullName)" -ForegroundColor Green
                } catch {
                    Write-Host "  ERROR al eliminar $($_.Name): $_" -ForegroundColor Red
                }
            }
    }
}

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "   LIMPIEZA COMPLETADA" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "`nAhora ejecuta: recompila con:"
Write-Host "  cd C:\Proyectos\MixCoach"
Write-Host "  cmake --build build --config Release"
Write-Host "`nLuego el script copiara los VST3 automaticamente a:"
Write-Host "  C:\Program Files\Common Files\VST3\"
Write-Host "`nFinalmente, en FL Studio:"
Write-Host "  1. Opciones -> Gestionar plugins"
Write-Host "  2. Busca MixCoach-1 y Messenger_2 en la lista"
Write-Host "  3. Click derecho -> Eliminar (o mover a Papelera)"
Write-Host "  4. Agrega las carpetas VST3 a la lista de escaneo"
Write-Host "  5. Haz clic en 'Escaneo rapido' o reinicia FL Studio"
Write-Host "============================================" -ForegroundColor Cyan
