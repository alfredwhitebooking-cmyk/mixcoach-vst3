# MixCoach - Compilar y Desplegar Plugins VST3
# Compila ambos plugins (MixCoach + Messenger) y copia los .vst3 a:
#   %LOCALAPPDATA%\Programs\Common\VST3\ (per-user, siempre funciona)
#   C:\Program Files\Common Files\VST3\  (sistema, requiere admin)
#
# NOTA: Los post-build steps del CMakeLists.txt ya copian automáticamente
# los .vst3 después de cada compilación. Este script es un wrapper
# opcional para compilar manualmente.

param(
    [switch]$SkipBuild,
    [switch]$Release = $true,
    [string]$BuildDir = "build"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MixCoach - Compilacion y Despliegue" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$PROJECT_DIR = "C:\Proyectos\MixCoach"
$BUILD_DIR = "$PROJECT_DIR\$BuildDir"
$CONFIG = if ($Release) { "Release" } else { "Debug" }

# ─── Destinos VST3 ────────────────────────────────────────────────────────────
$VST3_USER   = "$env:LOCALAPPDATA\Programs\Common\VST3"
$VST3_SYSTEM = "C:\Program Files\Common Files\VST3"

# 1. Configurar CMake (solo si no existe el build dir)
if (-not (Test-Path "$BUILD_DIR\CMakeCache.txt")) {
    Write-Host "[1/4] Configurando CMake..." -ForegroundColor Yellow
    Push-Location $PROJECT_DIR
    cmake -B $BUILD_DIR -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ Error en configuracion CMake" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
    Write-Host "  ✔ CMake configurado" -ForegroundColor Green
} else {
    Write-Host "[1/4] CMake ya configurado" -ForegroundColor Gray
}

# 2. Compilar
if (-not $SkipBuild) {
    Write-Host "[2/4] Compilando MixCoach + Messenger ($CONFIG)..." -ForegroundColor Yellow
    Write-Host "  (Esto puede tardar varios minutos...)" -ForegroundColor Gray
    Write-Host ""

    $errorFile = "$PROJECT_DIR\errores_compilacion.txt"
    $buildOutput = cmake --build $BUILD_DIR --config $CONFIG 2>&1

    # Guardar output completo
    $buildOutput | Out-File -FilePath $errorFile -Encoding UTF8

    $errorCount = ($buildOutput | Select-String -Pattern "error [A-Z]+\d+ :" -AllMatches).Matches.Count
    $warningCount = ($buildOutput | Select-String -Pattern "warning [A-Z]+\d+ :" -AllMatches).Matches.Count

    Write-Host ""
    Write-Host "  Resultados:" -ForegroundColor Yellow
    Write-Host "  Errores: $errorCount" -ForegroundColor $(if ($errorCount -gt 0) { "Red" } else { "Green" })
    Write-Host "  Warnings: $warningCount" -ForegroundColor $(if ($warningCount -gt 0) { "Yellow" } else { "Green" })

    if ($errorCount -gt 0) {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "  ❌ HAY ERRORES - Revisa:" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "  $errorFile" -ForegroundColor Cyan
        Write-Host ""
        pause
        exit 1
    }
} else {
    Write-Host "[2/4] Build saltado (-SkipBuild)" -ForegroundColor Gray
}

# 3. Copiar VST3 manualmente (por si los post-build steps fallaron)
Write-Host "[3/4] Copiando VST3 a directorios..." -ForegroundColor Yellow

$targets = @(
    @{ Name = "MixCoach"; Source = "$BUILD_DIR\MixCoach_artefacts\$CONFIG\VST3\MixCoach.vst3" },
    @{ Name = "Messenger"; Source = "$BUILD_DIR\Messenger_artefacts\$CONFIG\VST3\Messenger.vst3" }
)

$destinations = @(
    $VST3_USER
)

# Intentar copiar a sistema (puede fallar sin admin)
try {
    $destinations += $VST3_SYSTEM
} catch {
    Write-Host "  ⚠ No se puede acceder a $VST3_SYSTEM (sin permisos de admin)" -ForegroundColor Yellow
}

foreach ($target in $targets) {
    $src = $target.Source
    if (-not (Test-Path $src)) {
        Write-Host "  ⚠ No se encuentra: $src" -ForegroundColor Yellow
        continue
    }

    foreach ($dest in $destinations) {
        try {
            if (-not (Test-Path $dest)) {
                New-Item -ItemType Directory -Path $dest -Force | Out-Null
            }

            # ═══ LIMPIAR COPIAS RESIDUALES ═══════════════════════════════
            # FL Studio (y otros DAWs) a veces crean copias con sufijo
            # -1, -2 cuando detectan un plugin con mismo UID pero distinto
            # binario. Nuestros IDs únicos (MCch/Msgr) + VST3_CAN_REPLACE_VST2=0
            # evitan esto, pero limpiamos por si acaso.
            Write-Host "  Limpiando copias residuales viejas..." -ForegroundColor Gray
            $residualPatterns = @(
                "$dest\$($target.Name)-*.vst3",
                "$dest\$($target.Name)_*.vst3"
            )
            foreach ($pattern in $residualPatterns) {
                Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | ForEach-Object {
                    try {
                        Remove-Item -Path $_.FullName -Recurse -Force -ErrorAction SilentlyContinue
                        Write-Host "    🗑 Eliminada copia residual: $($_.Name)" -ForegroundColor DarkGray
                    } catch {
                        # Ignorar errores de permisos
                    }
                }
            }

            $destPath = "$dest\$($target.Name).vst3"
            if (Test-Path $destPath) {
                Remove-Item -Path $destPath -Recurse -Force
            }
            Copy-Item -Path $src -Destination $dest -Recurse -Force -ErrorAction Stop
            Write-Host "  ✔ $($target.Name).vst3 → $dest" -ForegroundColor Green
        } catch {
            Write-Host "  ⚠ $($target.Name).vst3 → $dest: $_" -ForegroundColor Yellow
        }
    }
}

# 4. Verificar resultado final
Write-Host ""
Write-Host "[4/4] Verificando plugins instalados..." -ForegroundColor Yellow

$allOk = $true
foreach ($target in $targets) {
    $found = $false
    foreach ($dest in $destinations) {
        $checkPath = "$dest\$($target.Name).vst3"
        if (Test-Path $checkPath) {
            $found = $true
            break
        }
    }
    if (-not $found) {
        Write-Host "  ✗ $($target.Name).vst3 NO instalado" -ForegroundColor Red
        $allOk = $false
    } else {
        Write-Host "  ✔ $($target.Name).vst3 instalado correctamente" -ForegroundColor Green
    }
}

Write-Host ""
if ($allOk) {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  ✅ TODO LISTO! Plugins instalados." -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Los plugins estan en:" -ForegroundColor Cyan
    foreach ($dest in $destinations) {
        Write-Host "  $dest" -ForegroundColor White
    }
    Write-Host ""
    Write-Host "Abre FL Studio y los plugins deberian cargarse sin duplicados." -ForegroundColor Cyan
} else {
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ❌ Algunos plugins no se instalaron." -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
}

Write-Host ""
pause
