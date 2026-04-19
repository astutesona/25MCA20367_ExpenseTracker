@echo off
title Smart Expense Tracker
color 0A

if not exist SmartExpenseTracker.exe (
    echo  SmartExpenseTracker.exe not found. Running build first...
    call build.bat
    if %errorlevel% neq 0 exit /b 1
)

echo.
echo  ============================================
echo   Starting Smart Expense Tracker...
echo   Browser will open automatically.
echo   Press Ctrl+C in this window to stop.
echo  ============================================
echo.

SmartExpenseTracker.exe
