@ECHO OFF
:: Remove project dir and "CMakeFiles" dir, if they exist; abort upon failure
IF EXIST %~dp0IDE rd /S /Q %~dp0IDE || GOTO abort
IF EXIST %~dp0CMakeFiles rd /S /Q %~dp0CMakeFiles || GOTO abort
:: Create project dir
mkdir %~dp0IDE || GOTO abort
:: Change current directory for cmake to generate its output to the correct folder
pushd %~dp0IDE
::Print cmake version
cmake --version | find /i "version"
:: Call cmake with the Visual Studio 2022 generator, and use the x64 toolset
cmake -G "Visual Studio 17 2022" -A "x64" -T host=x64 %~dp0

:: Change back to the original current directory
popd

GOTO exit

:abort
echo.
echo Script failed!

:exit