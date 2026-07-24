#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Feature Freeze Gate — Fase 0: Congelar desarrollo UX 2.0

.DESCRIPTION
    Detecta archivos nuevos en Source/MixCoach/UI/ que no están registrados
    en UX_APPROVED_COMPONENTS.yaml. Estos se consideran "nuevos features"
    y requieren aprobación UX explícita.

    También detecta nuevos archivos .h en engine/ y ai/ que no sean
    refactorizaciones de componentes existentes.

    Cómo aprobar un nuevo componente:
      1. Agregar entrada en UX_APPROVED_COMPONENTS.yaml con:
         - name, path, type, approved (fecha), by (quién aprueba), reason (justificación UX)
      2. Incluir UX_APPROVED_COMPONENTS.yaml en el commit
      3. El CI verificará que el componente esté registrado

    Exit codes:
      0 = all OK (no new unapproved components)
      1 = new unapproved components detected (block)
      2 = structural error

    Usage:
      .\scripts\check_feature_freeze.ps1
      .\scripts\check_feature_freeze.ps1 -ListFiles     # Solo listar archivos nuevos sin bloquear
      .\scripts\check_feature_freeze.ps1 -UpdateRegistry # Regenerar registry desde el código actual

.PARAMETER ListFiles
    Solo listar archivos nuevos sin verificar el registry (modo informativo)

.PARAMETER UpdateRegistry
    Regenerar el registry para reflejar el estado actual del código
    (útil después de aprobar componentes masivamente)
#>

param(
    [switch]$ListFiles,
    [switch]$UpdateRegistry
)

$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$RegistryPath = Join-Path $ProjectRoot "UX_APPROVED_COMPONENTS.yaml"
$ExitCode = 0

$Cyan = "Cyan"; $Green = "Green"; $Yellow = "Yellow"; $Red = "Red"; $Gray = "Gray"

# ─── Categorías de componentes monitoreadas ──────────────────────────────
$MonitoredDirs = @(
    "Source/MixCoach/UI",
    "Source/MixCoach/engine",
    "Source/MixCoach/ai"
)

# ─── Extensiones de archivo monitoreadas ─────────────────────────────────
$MonitoredExtensions = @(".h", ".cpp")

# ─── Archivos exentos (siempre permitidos) ───────────────────────────────
$ExemptFiles = @(
    "pch.h",
    "MixCoachTheme.h"     # Ya está registrado, pero es exempt por ser tema
)

# ─── Exentos por patrón de nombre ────────────────────────────────────────
$ExemptPatterns = @(
    '^.*_test\.(cpp|h)$',
    '^Test.*\.(cpp|h)$',
    '^.*Test\.(cpp|h)$'
)

# =========================================================================
#  FUNCIONES
# =========================================================================

function Get-ChangedFiles {
    <#
    .SYNOPSIS
        Obtiene archivos cambiados según el contexto:
        - En CI (GitHub Actions): compara contra el commit base del PR/commit
        - En pre-commit: archivos staged
    #>

    $files = @()

    # ─── Detectar entorno CI (GitHub Actions) ─────────────────────────
    $isCI = ($env:GITHUB_ACTIONS -eq "true") -or ($env:CI -eq "true")

    if ($isCI) {
        # En CI: comparar contra origin/main (PR) o HEAD~1 (push directo)
        # Primero intentar diff contra origin/main...HEAD (para PRs)
        $files = git diff --name-only --diff-filter=ACR "origin/main...HEAD" 2>$null
        if (-not $files) {
            # Fallback: diff contra HEAD~1 (push directo a develop)
            $files = git diff --name-only --diff-filter=ACR "HEAD~1" 2>$null
        }
        if (-not $files) {
            # Último fallback: diff contra el working tree
            $files = git diff --name-only --diff-filter=ACR 2>$null
        }
        Write-Host "  [CI] Modo GitHub Actions detectado" -ForegroundColor $Gray
        Write-Host "  [CI] Analizando cambios desde el commit base..." -ForegroundColor $Gray
    } else {
        # En pre-commit local: archivos staged
        $files = git diff --cached --name-only --diff-filter=ACR 2>$null
        Write-Host "  [Local] Modo pre-commit detectado" -ForegroundColor $Gray
        Write-Host "  [Local] Analizando archivos staged..." -ForegroundColor $Gray
    }

    return $files | Where-Object { $_ -match '\.(cpp|h)$' }
}

function Get-AllSourceFiles {
    <#
    .SYNOPSIS
        Obtiene TODOS los archivos .h/.cpp en los directorios monitoreados
    #>
    $files = @()
    foreach ($dir in $MonitoredDirs) {
        $fullDir = Join-Path $ProjectRoot $dir
        if (-not (Test-Path $fullDir)) { continue }
        $files += Get-ChildItem -Path $fullDir -Recurse -Include @("*.h", "*.cpp") `
            | Where-Object { -not $_.PSIsContainer } `
            | ForEach-Object { $_.FullName.Replace("$ProjectRoot\", "") }
    }
    return $files
}

function Get-RegisteredPaths {
    <#
    .SYNOPSIS
        Parsea UX_APPROVED_COMPONENTS.yaml y extrae todos los paths registrados.
        Expande shorthand como "File.h/.cpp" a "File.h" y "File.cpp".
    #>
    if (-not (Test-Path $RegistryPath)) {
        return @()
    }

    $yamlContent = Get-Content $RegistryPath -Raw

    # Extraer todas las líneas path:
    $pathMatches = [regex]::Matches($yamlContent, '(?m)^\s+path:\s+(.+)$')
    $allPaths = @()
    $foundUnregistered = $false

    foreach ($m in $pathMatches) {
        $relativePath = $m.Groups[1].Value.Trim()
        if ($relativePath -eq "" -or $relativePath -match '^#') { continue }

        # Expandir shorthand: "File.h/.cpp" -> "File.h" y "File.cpp"
        if ($relativePath -match '\.h/\.cpp$') {
            $basePath = $relativePath -replace '/\.cpp$', ''
            $allPaths += $basePath
            $cppPath = $basePath -replace '\.h$', '.cpp'
            $allPaths += $cppPath
        } else {
            $allPaths += $relativePath
        }
    }

    return $allPaths | Where-Object { $_ -ne "" } | ForEach-Object { $_.Trim() }
}

function Test-IsExempt {
    <#
    .SYNOPSIS
        Verifica si un archivo está exento del feature freeze
    #>
    param([string]$FilePath)

    # Exentos por nombre exacto
    foreach ($exempt in $ExemptFiles) {
        if ($FilePath -match [regex]::Escape($exempt)) { return $true }
    }

    # Exentos por patrón
    foreach ($pattern in $ExemptPatterns) {
        $filename = Split-Path $FilePath -Leaf
        if ($filename -match $pattern) { return $true }
    }

    # Siempre exento: el propio registry
    if ($FilePath -match 'UX_APPROVED_COMPONENTS\.yaml$') { return $true }

    # Siempre exento: tests
    if ($FilePath -match '^tests/') { return $true }

    return $false
}

function Test-IsMonitored {
    <#
    .SYNOPSIS
        Verifica si un archivo está en los directorios monitoreados
    #>
    param([string]$FilePath)

    $normalized = $FilePath.Replace('\', '/')
    foreach ($dir in $MonitoredDirs) {
        if ($normalized -match "^$dir/") { return $true }
    }
    return $false
}

function Test-FileExists {
    <#
    .SYNOPSIS
        Verifica si un archivo existe en disco
    #>
    param([string]$FilePath)
    $fullPath = Join-Path $ProjectRoot $FilePath
    return Test-Path $fullPath
}

function Update-Registry {
    <#
    .SYNOPSIS
        Regenera el registry desde el estado actual del código.
        Útil después de aprobar componentes masivamente.
    #>
    Write-Host "  Regenerando UX_APPROVED_COMPONENTS.yaml desde el código actual..." -ForegroundColor $Yellow

    $allFiles = Get-AllSourceFiles
    $registeredPaths = Get-RegisteredPaths

    $unregistered = @()
    foreach ($file in $allFiles) {
        if (Test-IsExempt -FilePath $file) { continue }
        $relative = $file.Replace('\', '/')

        $found = $false
        foreach ($rp in $registeredPaths) {
            if ($relative -eq $rp.Replace('\', '/')) {
                $found = $true
                break
            }
        }
        if (-not $found) {
            $unregistered += $relative
        }
    }

    if ($unregistered.Count -eq 0) {
        Write-Host "  ✅ No hay archivos sin registrar. Registry está al día." -ForegroundColor $Green
        return $true
    }

    Write-Host "  ⚠️  Se encontraron $($unregistered.Count) archivos sin registrar:" -ForegroundColor $Yellow
    foreach ($f in $unregistered) {
        Write-Host "    - $f" -ForegroundColor $Gray
    }

    Write-Host "  Para registrar, agrega manualmente las entradas en UX_APPROVED_COMPONENTS.yaml" -ForegroundColor $Yellow
    Write-Host "  con name, path, type, approved, by, y reason." -ForegroundColor $Yellow
    return $false
}

# =========================================================================
#  MAIN
# =========================================================================

Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor $Cyan
Write-Host "║     🔒 FEATURE FREEZE GATE — Fase 0: Congelar desarrollo  ║" -ForegroundColor $Cyan
Write-Host "║     No agregar features sin aprobación UX                  ║" -ForegroundColor $Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor $Cyan
Write-Host ""

# ─── Validar que el registry existe ──────────────────────────────────────
if (-not (Test-Path $RegistryPath)) {
    Write-Host "  ❌ UX_APPROVED_COMPONENTS.yaml no encontrado en:" -ForegroundColor $Red
    Write-Host "     $RegistryPath" -ForegroundColor $Red
    Write-Host "  → Crea el archivo con los componentes aprobados" -ForegroundColor $Yellow
    exit 2
}
Write-Host "  ✅ Registry: UX_APPROVED_COMPONENTS.yaml" -ForegroundColor $Green
Write-Host ""

# ─── Modo UpdateRegistry ─────────────────────────────────────────────────
if ($UpdateRegistry) {
    Update-Registry
    exit 0
}

# ─── Obtener archivos a verificar ────────────────────────────────────────
$changedFiles = Get-ChangedFiles

if ($changedFiles.Count -eq 0) {
    Write-Host "  📭 No hay archivos .cpp/.h nuevos o modificados. Feature freeze no aplica." -ForegroundColor $Gray
    exit 0
}

Write-Host "  Archivos a verificar: $($changedFiles.Count)" -ForegroundColor $Gray
Write-Host ""

# ─── Obtener paths registrados ───────────────────────────────────────────
$registeredPaths = Get-RegisteredPaths
Write-Host "  Componentes registrados: $($registeredPaths.Count)" -ForegroundColor $Gray
Write-Host ""

# ─── Detectar nuevas componentes UI/Engine/AI ────────────────────────────
Write-Host "▸ Escaneando nuevas componentes..." -ForegroundColor $Yellow

$unregisteredNewFiles = @()
$modifiedRegisteredFiles = @()

foreach ($file in $changedFiles) {
    $normalized = $file.Replace('\', '/')

    # Saltar si es exempt
    if (Test-IsExempt -FilePath $normalized) { continue }

    # Saltar si no está en directorios monitoreados (UI/, engine/, ai/)
    if (-not (Test-IsMonitored -FilePath $normalized)) { continue }

    # Verificar si el archivo existe en disco (puede ser un delete)
    if (-not (Test-FileExists -FilePath $normalized)) { continue }

    # Verificar si está registrado en el YAML
    $isRegistered = $false
    foreach ($rp in $registeredPaths) {
        if ($normalized -eq $rp.Replace('\', '/')) {
            $isRegistered = $true
            break
        }
    }

    if ($isRegistered) {
        $modifiedRegisteredFiles += $normalized
    } else {
        $unregisteredNewFiles += $normalized
    }
}

# ─── Reportar archivos modificados (OK, ya registrados) ─────────────────
if ($modifiedRegisteredFiles.Count -gt 0) {
    Write-Host "  ✅ Componentes existentes modificados: $($modifiedRegisteredFiles.Count)" -ForegroundColor $Green
    if ($ListFiles) {
        foreach ($f in $modifiedRegisteredFiles) {
            Write-Host "    ✓ $f" -ForegroundColor $Gray
        }
    }
}

# ─── Reportar archivos nuevos no registrados (BLOQUEADOS) ───────────────
if ($unregisteredNewFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "  ❌🔴 NUEVOS COMPONENTES NO APROBADOS: $($unregisteredNewFiles.Count)" -ForegroundColor $Red
    Write-Host ""  -ForegroundColor $Red
    foreach ($f in $unregisteredNewFiles) {
        Write-Host "    ✗ $f" -ForegroundColor $Red
    }
    Write-Host ""
    Write-Host "  ─────────────────────────────────────────────────────────────" -ForegroundColor $Yellow
    Write-Host "  🛑 FEATURE FREEZE VIOLATION" -ForegroundColor $Red
    Write-Host "  Fase 0: No agregar nuevos features sin aprobación UX." -ForegroundColor $Yellow
    Write-Host "" -ForegroundColor $Yellow
    Write-Host "  Para aprobar, edita UX_APPROVED_COMPONENTS.yaml:" -ForegroundColor $Yellow
    Write-Host "    - name: NombreDelComponente" -ForegroundColor $Gray
    Write-Host "      path: $($unregisteredNewFiles[0].Replace('.cpp', '.h/.cpp'))" -ForegroundColor $Gray
    Write-Host "      type: ui_component | analyzer | meter | panel | tool | utility | core" -ForegroundColor $Gray
    Write-Host "      approved: $(Get-Date -Format 'yyyy-MM-dd')" -ForegroundColor $Gray
    Write-Host "      by: [nombre de quien aprueba]" -ForegroundColor $Gray
    Write-Host "      reason: [justificación UX por qué es necesario]" -ForegroundColor $Gray
    Write-Host ""
    Write-Host "  Para bypass temporal: git commit --no-verify" -ForegroundColor $Yellow
    Write-Host "  ⚠️  NO USAR bypass sin aprobación UX explícita" -ForegroundColor $Red
    Write-Host "  ─────────────────────────────────────────────────────────────" -ForegroundColor $Yellow

    $ExitCode = 1
} else {
    Write-Host ""
    Write-Host "  ✅ Feature Freeze: TODOS los cambios están en componentes aprobados" -ForegroundColor $Green
}

# ─── Resumen ─────────────────────────────────────────────────────────────
Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor $Cyan
Write-Host "║  FEATURE FREEZE: $(if($ExitCode -eq 0){'✅ PASÓ'}else{'❌ BLOQUEÓ'})                                    ║" -ForegroundColor $(if($ExitCode -eq 0){$Green}else{$Red})
Write-Host "║  Modificados: $($modifiedRegisteredFiles.Count)  |  Nuevos no aprobados: $($unregisteredNewFiles.Count)  ║" -ForegroundColor $Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor $Cyan
Write-Host ""

exit $ExitCode
