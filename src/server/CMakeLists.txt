add_executable(server_app main.cpp session.cpp network_response_router.cpp)
target_link_libraries(server_app PRIVATE common)
