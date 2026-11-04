add_library(server STATIC
    # Исходники + все заголовки серверной логики
    entity.h
    entity_pool.h
    entity_utils.h
    event_bus.cpp
    event_bus.h
    network_context.h
    network_events.h
    network_response_router.cpp
    network_response_router.h
    session.cpp
    session.h
    session_registry.cpp
    session_registry.h
    tcp_server.cpp
    tcp_server.h
    thread_safe_queue.h
    world_instance.cpp
    world_instance.h
    asteroid_factory.cpp
    asteroid_factory.h
)

# PUBLIC: include-пути и зависимости транзитивны для server_app и тестов
target_include_directories(server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(server PUBLIC
    common
    Threads::Threads
)

add_executable(server_app main.cpp)
target_link_libraries(server_app PRIVATE server)
