cmake_minimum_required(VERSION 3.15.0)

if (CMAKE_C++_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
        link_libraries(stdc++fs)
    endif()
endif()

macro(configure_msvc_runtime)
    # Credit: https://stackoverflow.com/questions/10113017/setting-the-msvc-runtime-in-cmake
    if (MSVC)
        # Default to dynamically-linked runtime.
        if ("${MSVC_RUNTIME}" STREQUAL "")
            set(MSVC_RUNTIME "dynamic")
        endif ()

        # Set compiler options.
        set(variables
                CMAKE_C_FLAGS_DEBUG
                CMAKE_C_FLAGS_MINSIZEREL
                CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_DEBUG
                CMAKE_CXX_FLAGS_MINSIZEREL
                CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                )

        if (${MSVC_RUNTIME} STREQUAL "static")
            message(STATUS "MSVC -> forcing use of statically-linked runtime.")
            foreach (variable ${variables})
                if (${variable} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
                endif ()
            endforeach ()
        else ()
            message(STATUS "MSVC -> forcing use of dynamically-linked runtime.")
            foreach (variable ${variables})
                if (${variable} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
                endif ()
            endforeach ()
        endif ()
    endif ()
endmacro()