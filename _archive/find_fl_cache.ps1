Write-Host "=== BUSCANDO BASE DE DATOS DE PLUGINS DE FL STUDIO ===" -ForegroundColor Cyan

$paths = @(
    "$env:USERPROFILE\Documents\Image-Line",
    "$env:LOCALAPPDATA\Image-Line",
    "$env:APPDATA\Image-Line",
    "$env:PROGRAMDATA\Image-Line"
)

foreach ($base in $paths) {
    if (Test-Path $base) {
        Write-Host "`n--- $base ---" -ForegroundColor Yellow
        Get-ChildItem $base -Recurse -ErrorAction SilentlyContinue | 
            Where-Object { 
                $_.Name -match 'plugin|Plugin|cache|database|blacklist|scan|PluginState|State' -or 
                $_.Extension -match 'ini|fst|dat|db|xml|json|txt' 
            } | 
            ForEach-Object { 
                Write-Host "  $($_.FullName) ($($_.Length) B, $($_.LastWriteTime))" 
            }
    }
}

Write-Host "`n=== BUSCANDO ARCHIVOS RELACIONADOS A MixCoach/Messenger ===" -ForegroundColor Cyan
foreach ($base in $paths) {
    if (Test-Path $base) {
        Get-ChildItem $base -Recurse -ErrorAction SilentlyContinue | 
            Where-Object { $_.Name -match 'Messenger|MixCoach|xCoach|xCoachLabs' } | 
            ForEach-Object { 
                Write-Host "  ENCONTRADO: $($_.FullName) ($($_.Length) B)" 
            }
    }
}

Write-Host "`n=== BUSQUEDA COMPLETA EN Documents y AppData ===" -ForegroundColor Cyan
$allPaths = @(
    "$env:USERPROFILE\Documents",
    "$env:LOCALAPPDATA",
    "$env:APPDATA"
)
foreach ($base in $allPaths) {
    Get-ChildItem $base -Depth 2 -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match 'Image-Line|FL\s*Studio' -and $_.PSIsContainer } |
        ForEach-Object { Write-Host "  CARPETA: $($_.FullName)" }
}

Write-Host "`n=== FIN DE BUSQUEDA ===" -ForegroundColor Green
