#!/usr/bin/env bash
# ==============================================================================
# system-cleaner-agent - C++17 Autonomous ReAct Agent Build Script
# ==============================================================================

set -e

echo "Building system-cleaner-agent (C++17 ReAct Engine)..."

if command -v cmake &> /dev/null; then
    echo "[INFO] Using CMake to build project..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    cp system-cleaner-agent ../system-cleaner-agent 2>/dev/null || true
    cp SystemCleanerAgent ../SystemCleanerAgent 2>/dev/null || true
    cd ..
    echo "[SUCCESS] Build complete! Executable: ./system-cleaner-agent"
    exit 0
fi

if command -v g++ &> /dev/null; then
    echo "[INFO] Using G++ to build..."
    g++ -std=c++17 -O3 -I./include main.cpp -o system-cleaner-agent -lpthread
    echo "[SUCCESS] Build complete! Executable: ./system-cleaner-agent"
    exit 0
fi

echo "[ERROR] No compiler found. Install CMake, G++, or Clang++."
exit 1
