@echo off
setlocal
chcp 65001 >nul
set "VSLANG=1033"
set "TASK_ROOT=%~dp0.."
set "TASK_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%TASK_VSWHERE%" exit /b 1
for /f "usebackq tokens=*" %%v in (`"%TASK_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TASK_VS=%%v"
if not defined TASK_VS exit /b 1
call "%TASK_VS%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
set "TASK_CMAKE=%TASK_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "TASK_NINJA=%TASK_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
"%TASK_CMAKE%" -S "%TASK_ROOT%\native" -B "%TASK_ROOT%\build\native" -G Ninja "-DCMAKE_MAKE_PROGRAM=%TASK_NINJA%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
"%TASK_CMAKE%" --build "%TASK_ROOT%\build\native" --parallel 8
exit /b %errorlevel%
