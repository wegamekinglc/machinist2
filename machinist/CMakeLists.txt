file(GLOB_RECURSE MACHINIST_FILES "*.hpp" "*.cpp")
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*pattern.cpp$")
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*storable.cpp$")

add_library(machinistlib STATIC ${MACHINIST_FILES})

install(DIRECTORY . DESTINATION include/machinist
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS machinistlib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )