add_executable(unit_tests_common STUB_test_common.cpp)

# Линкуем тестируемую статическую библиотеку common и таргет gtest из Конана
target_link_libraries(unit_tests_common PRIVATE
    common
    GTest::gtest_main
)

# Интегрируем исполняемый файл в систему CTest для автоматического обнаружения тестов
include(GoogleTest)
gtest_discover_tests(unit_tests_common)
