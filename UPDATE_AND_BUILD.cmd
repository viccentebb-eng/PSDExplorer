@echo off
setlocal
cd /d "%~dp0"
where git >nul 2>nul
if errorlevel 1 (
  echo ERROR: Git no esta instalado o no esta en PATH.
  echo Instala Git for Windows y vuelve a intentarlo.
  pause
  exit /b 1
)
git rev-parse --is-inside-work-tree >nul 2>nul
if errorlevel 1 (
  echo ERROR: Esta carpeta todavia no es un repositorio Git clonado.
  echo Clona PSDExplorer desde GitHub una sola vez y despues usa este archivo.
  pause
  exit /b 1
)
echo.
echo == Actualizando PSD Explorer desde GitHub ==
git pull --ff-only
if errorlevel 1 goto :fail
echo.
echo == Compilando y ejecutando pruebas ==
call BUILD_WINDOWS.cmd
exit /b %errorlevel%
:fail
echo.
echo No se pudo actualizar con git pull. Revisa los mensajes anteriores.
pause
exit /b 1
