add_library(client STATIC
    tcp_client.cpp
    client_world.cpp
)

# Публично отдаём include-пути, чтобы все, кто линкуется к tcp_client, видели заголовки
target_include_directories(client PUBLIC
     ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(client PUBLIC
    common
    Boost::headers
    Threads::Threads
)

add_executable(client_app main.cpp)
target_link_libraries(client_app PRIVATE client)
