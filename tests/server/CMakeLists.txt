add_executable(unit_tests_server
        test_eventbus.cpp
        test_network_response_router.cpp
        test_thread_safe_queue.cpp
	test_tcp_server.cpp
	test_world_instance.cpp
	test_entity_health.cpp
	test_entity_utils.cpp
	test_score.cpp
    	test_ship_asteroid_collision.cpp
	test_ship_ship_collision.cpp
	test_bullet_ship_collision.cpp
)

target_link_libraries(unit_tests_server PRIVATE server common GTest::gmock GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests_server)
