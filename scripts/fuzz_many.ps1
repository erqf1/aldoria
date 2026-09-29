# Stresstest: führt die Zufallsskripte build\fuzz\fuzz_*.txt nacheinander aus.
# Erzeugen (Git Bash):  for s in 1 2 3; do bash scripts/make_fuzz.sh $s 450 > build/fuzz/fuzz_$s.txt; done
# Aufruf: powershell -File scripts\fuzz_many.ps1
$root = Resolve-Path "$PSScriptRoot\.."
$out = "$root\build\fuzz"
foreach ($f in Get-ChildItem "$out\fuzz_*.txt" | Sort-Object Name) {
    $env:ALDORIA_SAVES = "$out\saves_$($f.BaseName)"
    if (Test-Path $env:ALDORIA_SAVES) { Remove-Item -Recurse -Force $env:ALDORIA_SAVES }
    $p = Start-Process "$root\build\aldoria.exe" -ArgumentList "--script `"$($f.FullName)`" --shots `"$out`" --max-seconds 200" -WorkingDirectory $root -PassThru
    $done = $p.WaitForExit(260000)
    if (-not $done) { $p.Kill() }
    $errors = (Get-Content "$root\build\aldoria.log" -Encoding UTF8 | Where-Object { $_ -match "ERROR" })
    "$($f.Name): beendet=$done Exitcode=$($p.ExitCode) Fehlerzeilen=$($errors.Count)"
    $errors | Select-Object -First 5
}
