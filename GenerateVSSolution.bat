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

:: Detect system architecture
IF /I "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    SET "ARCHITECTURE=x64"
)
IF /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    SET "ARCHITECTURE=ARM64"
)

:: Call cmake with the Visual Studio 2022 generator, and use the detected toolset
cmake -G "Visual Studio 17 2022" -A "%ARCHITECTURE%" -T host=%ARCHITECTURE% %~dp0

:: Change back to the original current directory
popd

GOTO exit

:abort
echo.
echo Script failed!

:exit