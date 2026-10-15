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
        logger.h
        frame_codec.h
        math_utils.h
        vector2d.cpp
        types.h
        entity_snapshot.h
)

target_link_libraries(common PUBLIC Boost::boost)

# Boost.Asio на Windows требует ws2_32 & mswsock.
# PUBLIC — зависимость транзитивная: server_app, client_app и все тесты,
# линкующие common, получат ws2_32 автоматически.
if (WIN32)
    target_link_libraries(common PUBLIC ws2_32 mswsock)
endif ()

# FlatBuffers

find_package(flatbuffers CONFIG REQUIRED)

include(${PROJECT_SOURCE_DIR}/cmake/flatbuffers_codegen.cmake)

add_flatbuffers_schema(entity_snapshot_fbs
        "${PROJECT_SOURCE_DIR}/schemas/entity_snapshot.fbs")

target_link_libraries(common
        PRIVATE entity_snapshot_fbs
)
