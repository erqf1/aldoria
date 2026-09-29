# Startet das Spiel mit einem Testskript (siehe scripts/*.txt), wartet auf das Ende und zeigt das Log.
# Aufruf: powershell -File scripts\run_script.ps1 scripts\smoke.txt [Ausgabeordner] [-KeepSaves]
# Ohne -KeepSaves wird der Testspielstand-Ordner (im Ausgabeordner) vorher geleert.
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$Out = "$PSScriptRoot\..\build\shots",
    [switch]$KeepSaves
)
$root = Resolve-Path "$PSScriptRoot\.."
$exe = "$root\build\aldoria.exe"
$scriptPath = Resolve-Path $Script
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = Resolve-Path $Out
# Eigener Spielstand-Ordner, damit echte Spielstände unberührt bleiben
$env:ALDORIA_SAVES = "$Out\saves"
if (-not $KeepSaves -and (Test-Path "$Out\saves")) { Remove-Item -Recurse -Force "$Out\saves" }
$p = Start-Process $exe -ArgumentList "--script `"$scriptPath`" --shots `"$Out`" --max-seconds 180" -WorkingDirectory $root -PassThru
if (-not $p.WaitForExit(240000)) { $p.Kill(); "ZEITUEBERSCHREITUNG" } else { "Ende, Exitcode $($p.ExitCode)" }
Get-Content "$root\build\aldoria.log" -Encoding UTF8 | Where-Object { $_ -notmatch "Screenshot gespeichert" }
