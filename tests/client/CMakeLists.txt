add_executable(unit_tests_client test_client.cpp) # Путь без префикса src/

target_link_libraries(unit_tests_client PRIVATE
    common
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests_client)
