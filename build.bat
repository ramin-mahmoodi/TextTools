@echo off
setlocal

echo Checking for MinGW (g++)...
where g++ >nul 2>nul
if %errorlevel%==0 (
    echo MinGW found. Compiling with g++...
    windres resource.rc -o resource.o
    if %errorlevel% neq 0 (
        echo Failed to compile resources.
        exit /b 1
    )
    g++ -O3 -s -static -std=c++17 main.cpp processing.cpp resource.o -o TextTools.exe -mwindows -lcomctl32 -lole32 -luuid -ldwmapi -luxtheme
    if %errorlevel% neq 0 (
        echo Compilation failed.
        exit /b 1
    )
    echo Compilation successful! Executable is TextTools.exe
    exit /b 0
)

echo Checking for MSVC (cl.exe)...
where cl >nul 2>nul
if %errorlevel%==0 (
    echo MSVC found. Compiling with cl.exe...
    rc resource.rc
    if %errorlevel% neq 0 (
        echo Failed to compile resources.
        exit /b 1
    )
    cl /O2 /std:c++17 /EHsc main.cpp processing.cpp resource.res /link /SUBSYSTEM:WINDOWS comctl32.lib shell32.lib ole32.lib user32.lib /OUT:TextTools.exe
    if %errorlevel% neq 0 (
        echo Compilation failed.
        exit /b 1
    )
    echo Compilation successful! Executable is TextTools.exe
    exit /b 0
)

echo No suitable compiler found. Please ensure g++ (MinGW) or cl (MSVC) is in your PATH.
echo You can run this from "x64 Native Tools Command Prompt for VS" if using Visual Studio.
exit /b 1
