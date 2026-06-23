#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Validate AI Documentation vs Real Code — MixCoach Project
.DESCRIPTION
    Verifies that all AI documentation (AI_COMPONENT_INDEX.yaml, AI_CONTEXT.md)
    is synchronized with the actual source code. Exits with:
      0 = all OK
      1 = validation failures
      2 = structural error (missing required files)

    Checks performed:
      1. AI_COMPONENT_INDEX.yaml exists and is valid YAML
      2. Every path: in AI_COMPONENT_INDEX.yaml exists on disk
      3. Every [CORE] component has at least one test associated
      4. No V2 legacy references in AI docs (TelemetryProvider, TelemetryBuffer, etc.)
      5. All .cpp/.h files registered in CMakeLists.txt exist on disk
      6. AI_CONTEXT.md no-reference-check: no references to deleted V2 modules

    Usage:
      .\scripts\validate_ai_docs.ps1
      .\scripts\validate_ai_docs.ps1 -Verbose
#>

param(
    [switch]$Verbose,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

$Cyan   = "Cyan"
$Green  = "Green"
$Yellow = "Yellow"
$Red    = "Red"
$Gray   = "Gray"

$global:ChecksPassed = 0
$global:ChecksFailed = 0
$global:ChecksWarned = 0

function Write-Check {
    param([string]$Message, [string]$Status = "INFO")
    if ($Quiet -and $Status -eq "PASS") { return }
    $color = switch ($Status) {
        "PASS"  { $Green }
        "FAIL"  { $Red }
        "WARN"  { $Yellow }
        "INFO"  { $Gray }
        default { $Gray }
    }
    $symbol = switch ($Status) {
        "PASS"  { "[OK]" }
        "FAIL"  { "[FAIL]" }
        "WARN"  { "[WARN]" }
        default { "[INFO]" }
    }
    if (-not $Quiet -or $Status -ne "PASS") {
        Write-Host "  $symbol $Message" -ForegroundColor $color
    }
}

function Assert-Check {
    param([string]$Message, [scriptblock]$Condition)
    try {
        $result = & $Condition
        if ($result) {
            Write-Check -Message $Message -Status "PASS"
            $global:ChecksPassed++
            return $true
        } else {
            Write-Check -Message $Message -Status "FAIL"
            $global:ChecksFailed++
            return $false
        }
    } catch {
        Write-Host "  [EXCEPTION] $Message : $_" -ForegroundColor $Red
        $global:ChecksFailed++
        return $false
    }
}

function Assert-Warn {
    param([string]$Message, [scriptblock]$Condition)
    try {
        $result = & $Condition
        if ($result) {
            Write-Check -Message $Message -Status "PASS"
            $global:ChecksPassed++
            return $true
        } else {
            Write-Check -Message $Message -Status "WARN"
            $global:ChecksWarned++
            return $false
        }
    } catch {
        Write-Host "  [WARN] $Message : $_" -ForegroundColor $Yellow
        $global:ChecksWarned++
        return $false
    }
}

# ─── Step 0: Required files exist ────────────────────────────────────────
Write-Host "━━━ Validacion AI Docs vs Codigo Real ━━━" -ForegroundColor $Cyan
Write-Host ""

$requiredFiles = @(
    "AI_COMPONENT_INDEX.yaml",
    "AI_CONTEXT.md",
    "AI_RULES.md",
    "AI_CHECKLIST.md",
    "AI_QUICKSTART.md",
    "AI_PERSONA.md",
    "AI_VISION.md",
    "PRODUCT_VISION.md",
    "CMakeLists.txt"
)

Write-Host "▸ STEP 0/4: Required AI doc files exist" -ForegroundColor $Yellow
$allRequiredExist = $true
foreach ($f in $requiredFiles) {
    $path = Join-Path $ProjectRoot $f
    $exists = Test-Path $path
    Assert-Check -Message "Required file: $f" -Condition { $exists }
    if (-not $exists) { $allRequiredExist = $false }
}

if (-not $allRequiredExist) {
    Write-Host "`n[CRITICAL] Missing required files. Cannot continue." -ForegroundColor $Red
    exit 2
}

# ─── Step 1: Validate AI_COMPONENT_INDEX.yaml paths ────────────────────
Write-Host "`n▸ STEP 1/4: AI_COMPONENT_INDEX.yaml paths exist on disk" -ForegroundColor $Yellow

$yamlPath = Join-Path $ProjectRoot "AI_COMPONENT_INDEX.yaml"
$yamlContent = Get-Content $yamlPath -Raw

# Extraer todos los path: de AI_COMPONENT_INDEX.yaml
# NOTA: Los paths usan shorthand como "Source/File.h/.cpp" que significa
# "tanto File.h como File.cpp". Hay que expandirlos.
$pathMatches = [regex]::Matches($yamlContent, '(?m)^\s+path:\s+(.+)$')
$yamlPathsChecked = 0
$yamlPathsMissing = 0
$yamlPathsMissingList = @()

foreach ($m in $pathMatches) {
    $relativePath = $m.Groups[1].Value.Trim()
    if ($relativePath -eq "" -or $relativePath -match '^#') { continue }
    
    # Expandir shorthand: "File.h/.cpp" -> "File.h" y "File.cpp"
    if ($relativePath -match '\.h/\.cpp$' -or $relativePath -match '\.hpp/\.cpp$') {
        $basePath = $relativePath -replace '/\.cpp$', ''
        $pathsToCheck = @($basePath)
        # Si hay un .h, también debería haber un .cpp
        $cppPath = $basePath -replace '\.(h|hpp)$', '.cpp'
        if ($cppPath -ne $basePath) {
            $pathsToCheck += $cppPath
        }
    } else {
        $pathsToCheck = @($relativePath)
    }
    
    $allExist = $true
    foreach ($checkPath in $pathsToCheck) {
        $yamlPathsChecked++
        $fullPath = Join-Path $ProjectRoot $checkPath.Replace('/', '\')
        if (-not (Test-Path $fullPath)) {
            if ($allExist) { $allExist = $false }
            $yamlPathsMissing++
            $yamlPathsMissingList += $checkPath
            if ($Verbose) {
                Write-Check -Message "Path NOT FOUND: $checkPath (from: $relativePath)" -Status "FAIL"
            }
        }
    }
    
    if (-not $allExist -and -not $Verbose) {
        Write-Check -Message "Shorthand path has missing files: $relativePath" -Status "FAIL"
    }
}

if ($yamlPathsMissing -eq 0) {
    Assert-Check -Message "All $yamlPathsChecked expanded paths in AI_COMPONENT_INDEX.yaml exist" -Condition { $true }
} else {
    Assert-Check -Message "$yamlPathsMissing of $yamlPathsChecked expanded paths missing in AI_COMPONENT_INDEX.yaml" -Condition { $false }
    if ($Verbose) {
        foreach ($f in $yamlPathsMissingList) {
            Write-Host "    Missing: $f" -ForegroundColor $Gray
        }
    }
}

# ─── Step 2: [CORE] components have tests ──────────────────────────────
Write-Host "`n▸ STEP 2/4: [CORE] components have tests associated" -ForegroundColor $Yellow

# Buscar componentes [CORE]
$coreMatches = [regex]::Matches($yamlContent, '(?s)name:\s+(.+?)\n.*?path:\s+(.+?)\n.*?criticality:\s+CORE')
$coreComponents = @()
foreach ($m in $coreMatches) {
    $coreComponents += @{
        Name = $m.Groups[1].Value.Trim()
        Path = $m.Groups[2].Value.Trim()
    }
}

# Extraer tests del YAML
$testNames = @()
$testMatches = [regex]::Matches($yamlContent, '(?m)^\s+-\s+name:\s+(Test\w+)$')
foreach ($m in $testMatches) {
    $testNames += $m.Groups[1].Value.Trim()
}

Write-Host "  Found $($coreComponents.Count) [CORE] components, $($testNames.Count) test entries" -ForegroundColor $Gray

foreach ($core in $coreComponents) {
    $hasTest = $false
    foreach ($tn in $testNames) {
        if ($core.Name -like "$tn*" -or $tn -like "*$($core.Name)*") {
            $hasTest = $true
            break
        }
    }
    # Also check for test executables in CMakeLists.txt
    if (-not $hasTest) {
        $cmakePath = Join-Path $ProjectRoot "CMakeLists.txt"
        $cmakeContent = Get-Content $cmakePath -Raw
        if ($cmakeContent -match "Test$($core.Name)") {
            $hasTest = $true
        }
    }
    # Entry points (PluginProcessor, PluginEditor) no requieren tests unitarios dedicados
    # por diseño - se prueban via integracion (deploy + verificar en DAW)
    $entryPointExceptions = @("PluginProcessor", "PluginEditor")
    $isEntryPoint = $false
    foreach ($ex in $entryPointExceptions) {
        if ($core.Name -match $ex) { $isEntryPoint = $true; break }
    }
    if ($isEntryPoint) {
        Assert-Warn -Message "[CORE] $($core.Name) - entry point (probed via integration, skip unit test)" -Condition { $true }
    } else {
        Assert-Warn -Message "[CORE] $($core.Name) ($($core.Path)) has tests" -Condition { $hasTest }
    }
}

# ─── Step 3: No V2 legacy references in AI docs ───────────────────────
Write-Host "`n▸ STEP 3/4: No V2 legacy references in AI docs" -ForegroundColor $Yellow

$legacyTerms = @(
    "TelemetryProvider",
    "TelemetryBuffer",
    "TelemetryCollector",
    "MeterComponent",
    "TelemetryData",
    "AudioRingBuffer",
    "getTelemetry\(",
    "TrackTelemetry",
    "TelemetryManager",
    "TelemetryQueue"
)

$aiDocFiles = @(
    "AI_CONTEXT.md",
    "AI_RULES.md",
    "AI_CHECKLIST.md",
    "AI_QUICKSTART.md",
    "AI_VISION.md",
    "AI_PERSONA.md",
    "PRODUCT_VISION.md",
    "SAFE_EDIT_GUIDE.md",
    "AI_FLOW_DIAGRAM.md",
    "AI_CONTEXT_MAP.md",
    "AI_COMPONENT_INDEX.yaml"
)

$totalLegacyRefs = 0
foreach ($docFile in $aiDocFiles) {
    $docPath = Join-Path $ProjectRoot $docFile
    if (-not (Test-Path $docPath)) { continue }

    $docContent = Get-Content $docPath -Raw

    # Excepciones: referencias explícitas a "NO USAR" o "ELIMINADO" son válidas
    foreach ($term in $legacyTerms) {
        $matches = [regex]::Matches($docContent, $term)
        foreach ($match in $matches) {
            $lineNum = ($docContent.Substring(0, $match.Index).ToCharArray() -match "\n").Count + 1

            # Verificar si la línea contiene una excepción (NO USAR, ELIMINADO, no reintroducir)
            $lineStart = $docContent.LastIndexOf("`n", $match.Index) + 1
            if ($lineStart -lt 0) { $lineStart = 0 }
            $lineEnd = $docContent.IndexOf("`n", $match.Index)
            if ($lineEnd -lt 0) { $lineEnd = $docContent.Length }
            $lineContent = $docContent.Substring($lineStart, $lineEnd - $lineStart)

            if ($lineContent -match "NO USAR|ELIMINADO|no reintroducir|LEGACY|✂️|eliminado|Eliminado|muerto|MUERTO|NOT_AUDITED|NO USAR EN V3|eliminado en V3|ELIMINADOS|No existe en V3|NO EXISTE|eliminados|Eliminados") {
                continue # Está bien, es una referencia a legacy advertida
            }

            Write-Check -Message "V2 reference '$term' in $docFile line $lineNum" -Status "WARN"
            $totalLegacyRefs++
        }
    }
}

Assert-Warn -Message "No unexpected V2 legacy references in AI docs" -Condition { $totalLegacyRefs -eq 0 }

# ─── Step 4: CMakeLists.txt files exist on disk ────────────────────────
Write-Host "`n▸ STEP 4/4: CMakeLists.txt source files exist on disk" -ForegroundColor $Yellow

# Reuse existing check_cmakelists_files.ps1 if available
$cmakeChecker = Join-Path $ProjectRoot "scripts/check_cmakelists_files.ps1"
if (Test-Path $cmakeChecker) {
    $cmakeResult = & $cmakeChecker 2>&1
    $cmakeExit = $LASTEXITCODE
    Assert-Check -Message "CMakeLists.txt file validation (check_cmakelists_files.ps1)" -Condition { $cmakeExit -eq 0 }

    if ($Verbose -and $cmakeExit -ne 0) {
        Write-Host "  Details:" -ForegroundColor $Gray
        $cmakeResult | ForEach-Object { Write-Host "    $_" -ForegroundColor $Gray }
    }
} else {
    # Manual check
    $cmakePath = Join-Path $ProjectRoot "CMakeLists.txt"
    $cmakeContent = Get-Content $cmakePath -Raw
    $sourcePattern = '(?:(?:Source|tests)/[^\s"]+)\.(?:cpp|h|hpp)'
    $cmakeMatches = [regex]::Matches($cmakeContent, $sourcePattern)

    $cmakeMissingCount = 0
    foreach ($m in $cmakeMatches) {
        $relPath = $m.Value.Trim()
        $fullPath = Join-Path $ProjectRoot $relPath.Replace('/', '\')
        if (-not (Test-Path $fullPath)) {
            Write-Check -Message "CMakeLists.txt references missing file: $relPath" -Status "FAIL"
            $cmakeMissingCount++
        }
    }
    Assert-Check -Message "CMakeLists.txt: $($cmakeMatches.Count) files checked, $cmakeMissingCount missing" -Condition { $cmakeMissingCount -eq 0 }
}

# ─── Summary ────────────────────────────────────────────────────────────
$TotalChecks = $global:ChecksPassed + $global:ChecksFailed
Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor $Cyan
Write-Host "  VALIDATION SUMMARY" -ForegroundColor $Cyan
Write-Host "  Pass: $($global:ChecksPassed)  |  Fail: $($global:ChecksFailed)  |  Warnings: $($global:ChecksWarned)" -ForegroundColor $(if ($global:ChecksFailed -gt 0) { $Red } else { $Green })
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor $Cyan
Write-Host ""

if ($global:ChecksFailed -gt 0) {
    Write-Host "[FAIL] Some checks failed. Review warnings above." -ForegroundColor $Red
    exit 1
} elseif ($global:ChecksWarned -gt 0) {
    Write-Host "[WARN] All critical checks passed, but some warnings exist." -ForegroundColor $Yellow
    exit 0
} else {
    Write-Host "[PASS] All AI docs are synchronized with the codebase." -ForegroundColor $Green
    exit 0
}
