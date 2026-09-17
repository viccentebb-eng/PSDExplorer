@echo off
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo ERROR: CMake no esta en PATH. Instala Visual Studio con "Desktop development with C++" y CMake.
  pause
  exit /b 1
)
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
copy /y README.md dist\ >nul
copy /y CHANGES-1.1.2.txt dist\ >nul
powershell -NoProfile -Command "Compress-Archive -Path 'dist\*' -DestinationPath 'PSDExplorer-1.1.2-win-x64.zip' -Force"
echo.
echo LISTO: PSDExplorer-1.1.2-win-x64.zip
pause
exit /b 0
:fail
echo.
echo La compilacion fallo. Revisa los mensajes anteriores.
pause
exit /b 1
