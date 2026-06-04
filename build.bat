@echo off
g++ src/*.cpp -std=c++17 -pthread -I SDL2/include -I include -L SDL2/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -o game.exe

if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b
)

game.exe
pause