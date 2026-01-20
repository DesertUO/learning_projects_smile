@echo off

cls

gcc app.c utils.c -o OpenGL_1.exe -lglfw3 -lglew32 -lopengl32 -lgdi32 -luser32 -lkernel32

if "%~1"=="" (
    echo No arguments provided, running with 500 particles
    .\OpenGL_1.exe
) else (
    echo Particles count is: %1
    .\OpenGL_1.exe %1
)
