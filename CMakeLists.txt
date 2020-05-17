cmake_minimum_required(VERSION 3.0)

project(machinist VERSION 1.0.0)
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
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -stdlib=libc++" )
else()
    set(CMAKE_CXX_STANDARD 14)
endif()

if(NOT MSVC)
    find_package(Boost)
    if (Boost_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
    endif (Boost_FOUND)
endif()

add_subdirectory(machinist)
add_subdirectory(cli)