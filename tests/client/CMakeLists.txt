include(GoogleTest)

add_executable(tcp_client_tests
    test_tcp_client.cpp
)

target_link_libraries(tcp_client_tests PRIVATE
    client
    GTest::gtest_main
)

gtest_discover_tests(tcp_client_tests)