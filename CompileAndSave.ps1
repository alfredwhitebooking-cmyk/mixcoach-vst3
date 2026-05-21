# MixCoach - Compilar y Guardar Errores
# Copia los archivos corregidos, compila y guarda los errores en un .txt

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MixCoach - Compilación Automática" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. Copiar archivos corregidos
Write-Host "[1/3] Copiando archivos corregidos..." -ForegroundColor Yellow
$source = "C:\Windows\System32\MixCoach"
$dest = "C:\Proyectos\MixCoach"

if (Test-Path $source) {
    robocopy $source $dest /E /IS /NDL /NFL /NJH /NJS /R:1 /W:1 | Out-Null
    Write-Host "  ✔ Archivos copiados" -ForegroundColor Green
} else {
    Write-Host "  ⚠ No se encuentra la carpeta $source" -ForegroundColor Red
    Write-Host "  Los archivos ya deberían estar en $dest" -ForegroundColor Yellow
}

# 2. Ir al directorio del proyecto
Set-Location -Path $dest

# 3. Compilar y capturar errores
Write-Host "[2/3] Compilando MixCoach..." -ForegroundColor Yellow
Write-Host "  (Esto puede tardar varios minutos...)" -ForegroundColor Gray
Write-Host ""

$errorFile = "C:\Proyectos\MixCoach\errores_compilacion.txt"

# Ejecutar cmake --build y capturar TODO el output
$buildOutput = cmake --build build --config Release 2>&1

# Guardar output completo
$buildOutput | Out-File -FilePath $errorFile -Encoding UTF8

# Mostrar resumen
$errorCount = ($buildOutput | Select-String -Pattern "error [A-Z]+\d+ :" -AllMatches).Matches.Count
$warningCount = ($buildOutput | Select-String -Pattern "warning [A-Z]+\d+ :" -AllMatches).Matches.Count

Write-Host ""
Write-Host "[3/3] Resultados:" -ForegroundColor Yellow
Write-Host "  Errores: $errorCount" -ForegroundColor $(if ($errorCount -gt 0) { "Red" } else { "Green" })
Write-Host "  Warnings: $warningCount" -ForegroundColor $(if ($warningCount -gt 0) { "Yellow" } else { "Green" })

if ($errorCount -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  ✅ COMPILACIÓN EXITOSA!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Los plugins VST3 están en:" -ForegroundColor Cyan
    Write-Host "  C:\Proyectos\MixCoach\build\MixCoach_artefacts\Release\VST3\" -ForegroundColor White
    Write-Host "  C:\Proyectos\MixCoach\build\Messenger_artefacts\Release\VST3\" -ForegroundColor White
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "  ❌ HAY ERRORES - Archivo guardado" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Los errores están en:" -ForegroundColor Cyan
    Write-Host "  $errorFile" -ForegroundColor White
    Write-Host ""
    Write-Host "¿Qué hacer?" -ForegroundColor Yellow
    Write-Host "  1. Abre el archivo: notepad $errorFile" -ForegroundColor White
    Write-Host "  2. Presiona Ctrl+E, Ctrl+A (Seleccionar todo)" -ForegroundColor White
    Write-Host "  3. Presiona Ctrl+C (Copiar)" -ForegroundColor White
    Write-Host "  4. Pega aquí en el chat" -ForegroundColor White
}

Write-Host ""
pause
