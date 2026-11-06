find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

add_library(client STATIC
        tcp_client.cpp
        tcp_client.h
        client_world.cpp
        client_world.h
        input_provider.h
        renderer.h
        null_input_provider.h
        null_renderer.h
        sfml_renderer.h
        sfml_input_provider.h
        client_state.h
        game_controller.h
        game_controller.cpp
        score_view.h
        score_view.cpp
        leaderboard_view.h
        leaderboard_view.cpp
        death_screen.cpp
        death_screen.h
)

# Публично отдаём include-пути, чтобы все, кто линкуется к client, видели заголовки
target_include_directories(client PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_link_libraries(client PUBLIC
        common
        Boost::headers
        Threads::Threads
        sfml-graphics
        sfml-window
        sfml-system
)

add_executable(client_app main.cpp)
target_link_libraries(client_app PRIVATE client)
