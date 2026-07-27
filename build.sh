#!/usr/bin/env bash
# ==============================================================================
# Cross-Platform Build Script for Gemini Enterprise System Cleaner v2.5
# ==============================================================================

set -e

echo "Building Gemini Enterprise System Cleaner (C++17)..."

if command -v cmake &> /dev/null; then
    echo "[INFO] Using CMake to build project..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    cp gemini-sys-cleaner ../gemini-sys-cleaner 2>/dev/null || true
    cd ..
    echo "[SUCCESS] Build complete! Executable: ./gemini-sys-cleaner"
    exit 0
fi

if command -v g++ &> /dev/null; then
    echo "[INFO] Using G++ to build..."
    g++ -std=c++17 -O3 -I./include main.cpp -o gemini-sys-cleaner -lpthread
    echo "[SUCCESS] Build complete! Executable: ./gemini-sys-cleaner"
    exit 0
fi

if command -v clang++ &> /dev/null; then
    echo "[INFO] Using Clang++ to build..."
    clang++ -std=c++17 -O3 -I./include main.cpp -o gemini-sys-cleaner -lpthread
    echo "[SUCCESS] Build complete! Executable: ./gemini-sys-cleaner"
    exit 0
fi

echo "[ERROR] No compiler found. Install CMake, G++, or Clang++."
exit 1
