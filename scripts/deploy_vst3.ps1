$src = "C:\Proyectos\MixCoach\build\MixCoach_artefacts\Release\VST3\MixCoach.vst3"
$dst = "C:\Program Files\Common Files\VST3\"

Write-Output "Source: $src"
Write-Output "Dest: $dst"

# Remove old if exists
if (Test-Path ($dst + "MixCoach.vst3")) {
    Remove-Item -Recurse -Force ($dst + "MixCoach.vst3")
    Write-Output "Removed old"
}

# Copy fresh
Copy-Item -Path $src -Destination $dst -Recurse -Force
Write-Output "Copy done"

# Verify
$check = $dst + "MixCoach.vst3\moduleinfo.json"
if (Test-Path $check) {
    Write-Output "DEPLOY OK"
} else {
    Write-Output "DEPLOY FAILED"
}
