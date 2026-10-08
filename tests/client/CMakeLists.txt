find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(tcp_client_tests
    test_tcp_client.cpp

)

target_link_libraries(tcp_client_tests PRIVATE
    tcp_client
    GTest::GTest
    GTest::Main
)

gtest_discover_tests(tcp_client_tests)
