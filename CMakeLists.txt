cmake_minimum_required(VERSION 3.10)
project(GeminiSysCleaner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES main.cpp)
set(HEADERS Logger.hpp Cleaner.hpp)

add_executable(gemini-sys-cleaner ${SOURCES} ${HEADERS})

if(MSVC)
    set_property(TARGET gemini-sys-cleaner PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
