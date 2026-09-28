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

# Маппинг понятных имен на новые раздельные таргеты из CMakeLists.txt
case $COMPONENT in
    server) TARGETS="server_app" ;;
    client) TARGETS="client_app" ;;
    tests)  TARGETS="unit_tests_common unit_tests_server unit_tests_client" ;;
    *) echo "Ошибка: Неверный компонент. Допустимы: server, client, tests"; exit 1 ;;
esac # <--- ИСПРАВЛЕНО: Вместо fi теперь здесь правильно написано esac

# Имя конфигурации для вызова
if [ "$CONFIG" != "Debug" ] && [ "$CONFIG" != "Release" ]; then
    echo "Ошибка: Неверная конфигурация. Допустимы: Debug, Release"
    exit 1
fi

# КАЖДАЯ конфигурация использует собственную папку сборки (как в пресетах,
# которые генерирует Conan: binaryDir build/Debug и build/Release).
# Это критично: Conan раскладывает данные зависимостей (GTest, Boost) по
# конфигурациям, и CMake должен читать генераторы ИМЕННО своей конфигурации.
# Иначе (одна общая папка build/ с тулчейном Release) для Debug-сборки
# загружаются только Release-данные GTest — include-пути оказываются пустыми.
BUILD_DIR="build/$CONFIG"
GENERATORS_DIR="$BUILD_DIR/generators"

# Имя пресета конфигурирования из единого корневого CMakeUserPresets.json,
# который собирает Conan: debug-default / release-default
PRESET_NAME="$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')-default"

echo "========================================================="
echo " Настройка окружения Conan [$CONFIG]..."
echo "========================================================="
# Загружаем окружение Конана, соответствующее конфигурации (внутренний Ninja и пр.)
if [ -f "$GENERATORS_DIR/conanbuild.sh" ]; then
    source "$GENERATORS_DIR/conanbuild.sh"
else
    echo "Ошибка: Скрипты генераторов не найдены ($GENERATORS_DIR)."
    echo "Запустите сначала: conan install . --build=missing -s:a build_type=$CONFIG"
    exit 1
fi

# Однократная конфигурация CMake через единый пресет Конана.
# Пресет сам задаёт генератор Ninja Multi-Config, бинарную папку build/<Config>
# и тулчейн build/<Config>/generators/conan_toolchain.cmake.
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Конфигурация CMake [$CONFIG] через пресет '$PRESET_NAME'..."
    cmake --preset "$PRESET_NAME"
fi

# В цикле очищаем и собираем каждый таргет через нативные команды Ninja
for TARGET in $TARGETS; do
    echo "========================================================="
    echo " Очистка кэша сборки для таргета: $TARGET [$CONFIG]"
    echo "========================================================="
    # Ninja Multi-Config: внутри build/<Config> лежат правила ВСЕХ конфигураций,
    # а дефолтный phony-таргет (build.ninja) указывает на Debug-выходы.
    # Очистка через дефолтный файл удалила бы артефакты другой конфигурации,
    # поэтому чистим ИМЕННО свой конфигурационный файл build-<Config>.ninja.
    ninja -C "$BUILD_DIR" -f "build-$CONFIG.ninja" -t clean "$TARGET"

    echo "========================================================="
    echo " Сборка таргета: $TARGET [$CONFIG]"
    echo "========================================================="
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET"
done

# Если пересобирали тесты, сразу запускаем их через CTest в папке сборки
if [ "$COMPONENT" == "tests" ]; then
    echo "========================================================="
    echo " Запуск всех модульных тестов через Google Test..."
    echo "========================================================="
    cd "$BUILD_DIR"
    ctest -C "$CONFIG" --output-on-failure
    cd ../..
fi

echo "Успешно завершено! Бинарники находятся в out/$CONFIG/"
