add_executable(unit_tests_server test_server.cpp) # Путь без префикса src/

target_link_libraries(unit_tests_server PRIVATE
    common
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests_server)
