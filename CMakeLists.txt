cmake_minimum_required(VERSION 3.12.0)

project(machinist VERSION 1.0.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "") # works
include(${CMAKE_CURRENT_LIST_DIR}/cmake/machinist.cmake)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

message("-- Build Mode: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    set(MSVC_RUNTIME "dynamic" CACHE STRING "MSVC runtime to link")
    set_property(CACHE MSVC_RUNTIME PROPERTY STRINGS static dynamic)

    configure_msvc_runtime()

    # Remove warnings
    add_compile_options(/std:c++17)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_FLAGS "-fprofile-instr-generate -fcoverage-mapping")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
else()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_FLAGS "-ftest-coverage -fprofile-arcs")
    message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
endif()


add_subdirectory(machinist)
add_subdirectory(cli)
