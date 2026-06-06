@echo off
cd /d "%~dp0"

set "PATH=C:\msys64\mingw64\bin;%PATH%"

g++ src/*.cpp -std=c++17 -pthread ^
    -I"SDL2/include" ^
    -I"include" ^
    -I"C:\msys64\mingw64\include" ^
    -L"SDL2/lib" ^
    -L"C:\msys64\mingw64\lib" ^
    -o game.exe ^
    -lmingw32 -lSDL2main -lSDL2_image -lSDL2_ttf -lSDL2

if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b
)

game.exe
pause