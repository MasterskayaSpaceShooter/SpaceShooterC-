add_library(common STATIC)

# КРИТИЧЕСКИ ВАЖНО: Указываем текущую директорию (папку src/common) как путь поиска инклудов.
# Благодаря PUBLIC, все остальные модули (сервер, клиент, тесты) будут автоматически
# видеть заголовочные файлы библиотеки common при линковке.
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Все файлы лежат прямо в этой папке, без путей src/
target_sources(common PRIVATE
# Здесь перечисляем файлы реализации. Они компонуются в статическую библиотеку common ПРИМЕР:
    STUB_class.hpp
    STUB_class.cpp
)

target_link_libraries(common PUBLIC Boost::boost)