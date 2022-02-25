@echo off
FOR /F "TOKENS=1,2,*" %%A IN ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\App Paths\devenv.exe"') DO SET VSPATH=%%~dC%%~pC
@echo on

set CMAKE="%VSPATH%CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set BUILD_DIR="build_vs2022_64"

if not exist %BUILD_DIR% mkdir %BUILD_DIR%
pushd %BUILD_DIR%
%CMAKE% -D CMAKE_INSTALL_PREFIX=%VCPKG_DIR%/installed/x64-windows -G "Visual Studio 17 2022" .. 
popd
