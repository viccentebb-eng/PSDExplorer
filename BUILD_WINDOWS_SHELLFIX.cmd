@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build-win -A x64 -DPSD_EXPLORER_BUILD_TESTS=ON
if errorlevel 1 goto :fail
cmake --build build-win --config Release --parallel
if errorlevel 1 goto :fail
ctest --test-dir build-win -C Release --output-on-failure
if errorlevel 1 goto :fail
if exist dist rmdir /s /q dist
mkdir dist
copy /y build-win\Release\PSDExplorerShell.dll dist\ >nul
copy /y build-win\Release\PSDExplorerSetup.exe dist\ >nul
copy /y build-win\Release\PSDExplorerAdmin.exe dist\ >nul
copy /y README.md dist\ >nul
echo.
echo LISTO. Ejecuta dist\PSDExplorerSetup.exe
echo El archivo PSDExplorerAdmin.exe debe permanecer junto al instalador.
pause
exit /b 0
:fail
echo.
echo La compilacion fallo. Revisa los mensajes anteriores.
pause
exit /b 1
