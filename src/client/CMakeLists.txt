add_library(client STATIC
    tcp_client.cpp
    tcp_client.h
    client_world.cpp
    client_world.h
    input_provider.h
    renderer.h
    null_input_provider.h
    null_renderer.h
)

# Публично отдаём include-пути, чтобы все, кто линкуется к client, видели заголовки
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
