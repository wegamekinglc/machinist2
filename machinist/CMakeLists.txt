file(GLOB_RECURSE MACHINIST_FILES "*.hpp" "*.cpp")
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*pattern.cpp$")
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*storable.cpp$")

add_library(machinist STATIC ${MACHINIST_FILES})

if(NOT MSVC)
    find_package (Boost COMPONENTS filesystem regex REQUIRED)
    target_link_libraries (machinist ${Boost_LIBRARIES})
endif()

install(DIRECTORY . DESTINATION include/machinist
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS machinist
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )