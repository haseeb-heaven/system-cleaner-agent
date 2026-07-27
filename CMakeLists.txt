cmake_minimum_required(VERSION 3.14)
project(system-cleaner-agent VERSION 5.3.1 LANGUAGES CXX)

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
    include/SmartScheduler.hpp
    include/LocalLLMBrain.hpp
    include/SecurityGuard.hpp
    include/AgentQueryLanguage.hpp
    include/TaskHistory.hpp

    include/gtlibc.hpp
)

set(SOURCES
    include/gtlibc.cpp
)

# Single Canonical Target Executable
add_executable(system-cleaner-agent main.cpp ${HEADERS} ${SOURCES})

# Comprehensive Unit Test Executable
add_executable(unit_tests tests/test_main.cpp ${HEADERS} ${SOURCES})

enable_testing()
add_test(NAME system-cleaner-agent-tests COMMAND unit_tests)

if(WIN32)
    target_link_libraries(system-cleaner-agent PRIVATE shell32)
    target_link_libraries(unit_tests PRIVATE shell32)
    if(MSVC)
        set_property(TARGET system-cleaner-agent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET unit_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # Embed Windows icon and version info resource into the main executable
    set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc")
    if(EXISTS "${RC_FILE}")
        configure_file("${RC_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" COPYONLY)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.ico"
            "${CMAKE_CURRENT_BINARY_DIR}/app_icon.ico"
            COPYONLY
        )
        target_sources(system-cleaner-agent PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(system-cleaner-agent PRIVATE pthread)
    target_link_libraries(unit_tests PRIVATE pthread)
endif()
