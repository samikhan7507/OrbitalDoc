@echo off
REM Build the WebAssembly version -> build_web\index.html   (run from an emsdk_env.bat shell)
REM Author: Sami Khan
where emcc >nul 2>nul || (echo emcc not found. Install Emscripten and run emsdk_env.bat first. & exit /b 1)
if not exist build_web mkdir build_web
emcc -std=c++17 -O3 -Isrc src\main.cpp src\hud.cpp src\engine\renderer.cpp src\engine\gfx.cpp -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sENVIRONMENT=web -sEXPORTED_FUNCTIONS=_main,_set_touch_input,_ui_action,_orbit_drag,_orbit_zoom --shell-file web\shell.html -o build_web\index.html
echo Built build_web\index.html - now run: python serve.py
