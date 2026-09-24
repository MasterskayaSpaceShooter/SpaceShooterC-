cmake_minimum_required(VERSION 3.20)
project(SpaceShooter VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Для простого подключения intellisence
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
#   Global output directories
# -----------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(PROJECT_ROOT ${CMAKE_SOURCE_DIR})

# -----------------------------------------------------------------------------
#   Add subprojects
# -----------------------------------------------------------------------------
add_subdirectory(client)
add_subdirectory(common)
add_subdirectory(server)

option(BUILD_TESTS "Build tests" ON)
if (BUILD_TESTS)
    enable_testing()
endif ()
