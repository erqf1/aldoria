@echo off
rem Erzeugt einen Ordner dist\Aldoria zum Weitergeben (exe + data + README). Vorher build.bat ausfuehren.
if not exist build\aldoria.exe (
    echo build\aldoria.exe fehlt. Erst build.bat ausfuehren.
    exit /b 1
)
if exist dist\Aldoria rmdir /s /q dist\Aldoria
mkdir dist\Aldoria || exit /b 1
copy /y build\aldoria.exe dist\Aldoria\ >nul || exit /b 1
xcopy /e /i /q /y data dist\Aldoria\data >nul || exit /b 1
copy /y README.md dist\Aldoria\ >nul
echo.
echo Fertig: dist\Aldoria\aldoria.exe (Spielstaende landen in dist\Aldoria\saves)
