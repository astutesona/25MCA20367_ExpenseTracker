@echo off
title Smart Expense Tracker — Build
color 0B
echo.
echo  ============================================
echo   Smart Expense Tracker — Building...
echo  ============================================
echo.

REM Check if g++ is available
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo  ERROR: g++ not found. Please install MinGW.
    echo  Add C:\MinGW\bin to your PATH.
    pause
    exit /b 1
)

echo  Step 1: Compiling SQLite database engine (C)...
gcc -std=c11 -O2 -c sqlite3.c -o sqlite3.o -DSQLITE_THREADSAFE=0
if %errorlevel% neq 0 (
    echo  [FAILED] SQLite compilation failed.
    pause
    exit /b 1
)

echo  Step 2: Compiling C++ source and linking...
g++ -std=c++14 -O2 ^
    -o SmartExpenseTracker.exe ^
    main.cpp sqlite3.o ^
    -lws2_32 ^
    -I. ^
    -static-libgcc -static-libstdc++ ^
    -DWIN32 -D_WIN32_WINNT=0x0601

if %errorlevel% neq 0 (
    echo.
    echo  [FAILED] Build failed. Check errors above.
    pause
    exit /b 1
)

echo.
echo  ============================================
echo   Build successful!
echo   Output: SmartExpenseTracker.exe  (~3.5 MB)
echo  ============================================
echo.
echo  Run run.bat to start the application.
echo.
pause
