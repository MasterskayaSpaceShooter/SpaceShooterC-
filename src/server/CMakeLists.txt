add_executable(server_app main.cpp session.cpp session_registry.cpp)
target_link_libraries(server_app PRIVATE common)
