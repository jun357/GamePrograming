@echo off
g++ main.cpp -I SDL2/include -L SDL2/lib -lmingw32 -lSDL2main -lSDL2 -o game.exe
g++ main.cpp -I SDL2/include -L SDL2/lib -lmingw32 -lSDL2main -lSDL2 -o game.exe

if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b
)

game.exe
pause