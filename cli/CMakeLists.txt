file(GLOB_RECURSE MACHINIST_FILES "${CMAKE_SOURCE_DIR}/machinist/*.cpp" "${CMAKE_SOURCE_DIR}/machinist/*.hpp")
include_directories(${CMAKE_SOURCE_DIR}/machinist)
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*pattern.cpp$")
list(FILTER MACHINIST_FILES EXCLUDE REGEX ".*storable.cpp$")
add_executable(Machinist machinist.cpp ${MACHINIST_FILES})


install(TARGETS Machinist
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )