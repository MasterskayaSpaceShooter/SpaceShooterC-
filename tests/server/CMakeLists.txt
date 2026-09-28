add_executable(unit_tests_server  tests_eventbus.cpp) # Путь без префикса src/

target_include_directories(unit_tests_server PRIVATE ${CMAKE_SOURCE_DIR}/src/server)

target_link_libraries(unit_tests_server  PRIVATE
    common
    GTest::gmock
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests_server)
