add_executable(unit_tests_server
        test_eventbus.cpp
        test_network_response_router.cpp
        test_thread_safe_queue.cpp
        ${CMAKE_SOURCE_DIR}/src/server/network_response_router.cpp
)

target_include_directories(unit_tests_server PRIVATE ${CMAKE_SOURCE_DIR}/src/server)

target_link_libraries(unit_tests_server PRIVATE
        common
        GTest::gmock
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests_server)
