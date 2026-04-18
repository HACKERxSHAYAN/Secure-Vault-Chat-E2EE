@echo off
echo 
echo Secure-Vault-Chat Build Script
echo 
echo.

REM Check for MinGW-w64 g++
where g++ >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [*] Found MinGW-w64 g++ compiler
    echo [*] Compiling server...
    g++ -std=c++17 -static -O2 server.cpp -o server.exe -lws2_32
    if %ERRORLEVEL% neq 0 (
        echo [!] Server compilation failed!
        pause
        exit /b 1
    )
    
    echo [*] Compiling client...
    g++ -std=c++17 -static -O2 client.cpp -o client.exe -lws2_32
    if %ERRORLEVEL% neq 0 (
        echo [!] Client compilation failed!
        pause
        exit /b 1
    )
    
    echo.
    echo [+] Build complete! Run server.exe and client.exe
    pause
    exit /b 0
)

REM Check for MSVC cl.exe
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [*] Found MSVC compiler
    echo [*] Compiling server...
    cl /EHsc /std:c++17 server.cpp /link ws2_32.lib
    if %ERRORLEVEL% neq 0 (
        echo [!] Server compilation failed!
        pause
        exit /b 1
    )
    
    echo [*] Compiling client...
    cl /EHsc /std:c++17 client.cpp /link ws2_32.lib
    if %ERRORLEVEL% neq 0 (
        echo [!] Client compilation failed!
        pause
        exit /b 1
    )
    
    echo.
    echo [+] Build complete! Run server.exe and client.exe
    pause
    exit /b 0
)

echo [!] No C++ compiler found.
echo.
echo To compile this project, install one of:
echo   - MinGW-w64 (https://www.mingw-w64.org/)
echo   - Visual Studio 2019+ with C++ workload
echo   - MSYS2 with mingw-w64
echo.
pause
exit /b 1