add_library(tcp_client STATIC
    tcp_client.cpp
)

# Публично отдаём include-пути, чтобы все, кто линкуется к tcp_client, видели заголовки
target_include_directories(tcp_client PUBLIC
     ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tcp_client PUBLIC
    common
    Boost::headers
    pthread
)

add_executable(client_app main.cpp)
target_link_libraries(client_app PRIVATE tcp_client)
