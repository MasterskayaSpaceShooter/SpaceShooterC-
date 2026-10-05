add_executable(server_app
    main.cpp
    session.cpp
    session_registry.cpp
    session_registry.h
    network_response_router.cpp
    network_response_router.h
)
target_link_libraries(server_app PRIVATE common)
