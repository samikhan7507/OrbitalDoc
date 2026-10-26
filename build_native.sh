#!/usr/bin/env bash
# Build the native desktop version (Linux/macOS). Needs a C++17 compiler + GLFW3 dev package.
#   Ubuntu/Debian: sudo apt install build-essential libglfw3-dev     macOS: brew install glfw
# Author: Sami Khan
set -e
cd "$(dirname "$0")"
mkdir -p build_native
SRC="src/main.cpp src/hud.cpp src/engine/renderer.cpp src/engine/gfx.cpp"
if [[ "$(uname)" == "Darwin" ]]; then
  c++ -std=c++17 -O2 -Isrc $SRC $(pkg-config --cflags --libs glfw3) -framework OpenGL -o build_native/orbitaldock
else
  c++ -std=c++17 -O2 -Isrc $SRC $(pkg-config --cflags --libs glfw3) -ldl -o build_native/orbitaldock
fi
echo "Built build_native/orbitaldock  ->  run: ./build_native/orbitaldock"
