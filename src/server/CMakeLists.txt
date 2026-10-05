add_executable(server_app
    main.cpp
    session.cpp
    network_response_router.cpp
    network_response_router.h
)
target_link_libraries(server_app PRIVATE common)
