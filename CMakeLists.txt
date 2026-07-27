cmake_minimum_required(VERSION 3.14)
project(GeminiSysCleaner VERSION 2.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SOURCES main.cpp)
set(HEADERS
    include/Logger.hpp
    include/ContentInspector.hpp
    include/ProcessManager.hpp
    include/Cleaner.hpp
)

add_executable(gemini-sys-cleaner ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(gemini-sys-cleaner PRIVATE shell32)
    if(MSVC)
        set_property(TARGET gemini-sys-cleaner PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(gemini-sys-cleaner PRIVATE pthread)
endif()
