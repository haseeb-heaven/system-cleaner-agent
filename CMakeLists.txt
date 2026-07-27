cmake_minimum_required(VERSION 3.14)
project(system-cleaner-agent VERSION 4.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(HEADERS
    include/Logger.hpp
    include/ContentInspector.hpp
    include/ProcessManager.hpp
    include/Cleaner.hpp
    include/OpenTUI.hpp
    include/TUI.hpp
    include/AgentEngine.hpp
)

# Agentic System Executables
add_executable(system-cleaner-agent main.cpp ${HEADERS})
add_executable(SystemCleanerAgent main.cpp ${HEADERS})

# Comprehensive Unit Test Executable
add_executable(unit_tests tests/test_main.cpp ${HEADERS})

enable_testing()
add_test(NAME SystemCleanerAgentUnitTests COMMAND unit_tests)

if(WIN32)
    target_link_libraries(system-cleaner-agent PRIVATE shell32)
    target_link_libraries(SystemCleanerAgent PRIVATE shell32)
    target_link_libraries(unit_tests PRIVATE shell32)
    if(MSVC)
        set_property(TARGET system-cleaner-agent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET SystemCleanerAgent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET unit_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(system-cleaner-agent PRIVATE pthread)
    target_link_libraries(SystemCleanerAgent PRIVATE pthread)
    target_link_libraries(unit_tests PRIVATE pthread)
endif()
