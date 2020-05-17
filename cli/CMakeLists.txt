add_executable(Machinist machinist.cpp)
target_link_libraries(Machinist machinist)

install(TARGETS Machinist
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )