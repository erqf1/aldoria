@echo off
rem Baut das Spiel mit den Visual Studio Build Tools. Ergebnis: build\rpg.exe
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || goto :err
cmake --build build || goto :err
echo.
echo Fertig: build\rpg.exe
goto :eof

:err
echo.
echo Build fehlgeschlagen.
exit /b 1
