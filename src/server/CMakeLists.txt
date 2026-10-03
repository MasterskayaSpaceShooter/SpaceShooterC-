add_executable(server_app main.cpp session.cpp network_context.h)
target_link_libraries(server_app PRIVATE common)
