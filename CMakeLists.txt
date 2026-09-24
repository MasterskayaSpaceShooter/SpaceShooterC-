cmake_minimum_required(VERSION 3.25)
project(SpaceShooter VERSION 0.1 LANGUAGES CXX)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
endif()

# Экспорт compile_commands.json для корректной работы IntelliSense в IDE
# (VS Code C/C++, clangd и пр.): редактор получит include-пути Boost/GTest из Conan
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Глобальное перенаправление вывода бинарников в корень проекта в папку out/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Макрос для Boost.Asio на Windows 10+
endif()

# Находим пакет Boost, предоставленный Conan
find_package(Boost REQUIRED COMPONENTS headers)

# Переходим в папку с исходным кодом
add_subdirectory(src)

# Настройка тестов
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        find_package(gtest REQUIRED CONFIG)
        add_subdirectory(tests)
    endif()
endif()
