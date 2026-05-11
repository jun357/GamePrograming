@echo off
g++ src/*.cpp -I SDL2/include -L SDL2/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -o game.exe

if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b
)

game.exe
pause