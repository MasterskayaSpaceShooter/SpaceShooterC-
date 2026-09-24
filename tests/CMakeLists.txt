add_executable(unit_tests src/test_server.cpp)
target_link_libraries(unit_tests PRIVATE common GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(unit_tests)
