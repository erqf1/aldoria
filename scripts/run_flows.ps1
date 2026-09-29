# Führt alle Ablaufskripte scripts\flow_*.txt nacheinander aus und meldet Exitcode und Fehlerzeilen im Log.
# Aufruf: powershell -File scripts\run_flows.ps1 [Muster]      z. B. flow_boss*
param([string]$Pattern = "flow_*.txt")
$root = Resolve-Path "$PSScriptRoot\.."
$out = "$root\build\flows"
New-Item -ItemType Directory -Force $out | Out-Null
$bad = 0
foreach ($f in Get-ChildItem "$PSScriptRoot\$Pattern" | Sort-Object Name) {
    $env:ALDORIA_SAVES = "$out\saves_$($f.BaseName)"
    if (Test-Path $env:ALDORIA_SAVES) { Remove-Item -Recurse -Force $env:ALDORIA_SAVES }
    $p = Start-Process "$root\build\aldoria.exe" -ArgumentList "--script `"$($f.FullName)`" --shots `"$out\$($f.BaseName)`" --max-seconds 200" -WorkingDirectory $root -PassThru
    $done = $p.WaitForExit(260000)
    if (-not $done) { $p.Kill() }
    $errors = @(Get-Content "$root\build\aldoria.log" -Encoding UTF8 | Where-Object { $_ -match "ERROR" -or $_ -match "abgest" })   # abgestürzte Gegner zählen als Fehler
    $ok = $done -and $p.ExitCode -eq 0 -and $errors.Count -eq 0
    if (-not $ok) { $bad++ }
    "{0,-24} beendet={1} Exitcode={2} Fehlerzeilen={3} {4}" -f $f.Name, $done, $p.ExitCode, $errors.Count, $(if ($ok) { "OK" } else { "PROBLEM" })
    $errors | Select-Object -First 5
}
if ($bad -gt 0) { "Probleme: $bad" } else { "Alle Abläufe fehlerfrei" }
