#!/bin/bash
# Останавливать скрипт при любой возникшей ошибке
set -e

# Проверяем аргументы командной строки
if [ "$#" -ne 2 ]; then
    echo "Использование: $0 [server|client|tests] [Debug|Release]"
    echo "Пример: $0 server Debug"
    exit 1
fi

COMPONENT=$1
CONFIG=$2

# Маппинг понятных имен на таргеты из CMakeLists.txt
case $COMPONENT in
    server) TARGET="server_app" ;;
    client) TARGET="client_app" ;;
    tests)  TARGET="unit_tests" ;;
    *) echo "Ошибка: Неверный компонент. Допустимы: server, client, tests"; exit 1 ;;
esac

# Проверка корректности конфигурации
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Ошибка: Неверная конфигурация. Допустимы: Debug, Release"
    exit 1
fi

echo "========================================================="
echo " Настройка окружения Conan и проверка Ninja..."
echo "========================================================="
# Загружаем мультиконфигурационное окружение Конана (берем из папки Release)
if [ -f "build/Release/generators/conanbuild.sh" ]; then
    source build/Release/generators/conanbuild.sh
else
    echo "Ошибка: Скрипты генераторов не найдены. Запустите сначала 'conan install .'"
    exit 1
fi

# Если CMake проект ещё не был сконфигурирован, запускаем генерацию пресетов
if [ ! -d "build" ] || [ ! -f "build/build-Debug.ninja" ]; then
    echo "Первоначальная конфигурация CMake через пресеты..."
    cmake --preset conan-default
fi

echo "========================================================="
echo " Очистка кэша сборки для таргета: $TARGET [$CONFIG]"
echo "========================================================="
# Вызываем встроенный инструмент очистки CMake/Ninja строго для одной цели
cmake --build --preset conan-default --config $CONFIG --target $TARGET/clean

echo "========================================================="
echo " Сборка таргета: $TARGET [$CONFIG]"
echo "========================================================="
cmake --build --preset conan-default --config $CONFIG --target $TARGET

# Если пересобирали тесты, сразу запускаем их через корректный пресет CTest
if [ "$COMPONENT" == "tests" ]; then
    echo "========================================================="
    echo " Запуск модульных тестов через Google Test..."
    echo "========================================================="
    # Переводим имя конфигурации в нижний регистр для пресета ctest (conan-debug / conan-release)
    TEST_PRESET="conan-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
    ctest --preset $TEST_PRESET --output-on-failure
fi

echo "Успешно завершено! Бинарник находится в out/$CONFIG/"
