@echo off
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release --target AnA_VST3
if errorlevel 1 exit /b 1
echo Done.
pause
