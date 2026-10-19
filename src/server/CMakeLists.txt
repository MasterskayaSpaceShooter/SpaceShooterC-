add_executable(server_app
        main.cpp
        session.cpp
        session_registry.cpp
        session_registry.h
        network_response_router.cpp
        network_response_router.h
        network_context.h
        tcp_server.cpp
        entity.h
        entity_pool.h
        entity_utils.h
        asteroid_factory.h
        asteroid_factory.cpp
)

target_link_libraries(server_app PRIVATE common)
