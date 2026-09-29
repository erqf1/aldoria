@echo off
rem Baut Aldoria (Spiel + Tests) mit den Visual Studio Build Tools und startet die Tests.
rem Ergebnis: build\aldoria.exe
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || goto :err
cmake --build build || goto :err
ctest --test-dir build --output-on-failure || goto :err
echo.
echo Fertig: build\aldoria.exe
goto :eof

:err
echo.
echo Build oder Tests fehlgeschlagen.
exit /b 1
