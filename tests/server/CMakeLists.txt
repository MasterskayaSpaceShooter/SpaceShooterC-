# tests/server/CMakeLists.txt

# --- ТЕСТ ДЛЯ EVENT BUS ---
add_executable(event_bus_tests test_eventbus.cpp)

target_include_directories(event_bus_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(event_bus_tests PRIVATE
    common
    GTest::gtest_main

)

include(GoogleTest)
gtest_discover_tests(event_bus_tests)


# --- ТЕСТ ДЛЯ SERVER ---
add_executable(unit_tests_server test_server.cpp)

target_include_directories(unit_tests_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/common
)

target_link_libraries(unit_tests_server PRIVATE
    common
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests_server)
