#!/usr/bin/env bash
# Build the WebAssembly version -> build_web/index.html   (needs the Emscripten SDK: emcc)
# Author: Sami Khan
set -e
cd "$(dirname "$0")"
command -v emcc >/dev/null || { echo "emcc not found. Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"; exit 1; }
mkdir -p build_web
emcc -std=c++17 -O3 -Isrc \
  src/main.cpp src/hud.cpp src/engine/renderer.cpp src/engine/gfx.cpp \
  -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sENVIRONMENT=web \
  -sEXPORTED_FUNCTIONS=_main,_set_touch_input,_ui_action,_orbit_drag,_orbit_zoom \
  --shell-file web/shell.html \
  -o build_web/index.html
echo "Built build_web/index.html  ->  run:  python3 serve.py   then open http://localhost:8000"
