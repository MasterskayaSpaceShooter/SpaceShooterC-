#!/bin/bash
# run_tests.sh — запуск выбранных модульных тестов проекта.
#
# Использование:
#   ./run_tests.sh [Debug|Release] <имя_теста> [<имя_теста> ...]
#
# Примеры:
#   ./run_tests.sh NetworkResponseRouterTest.SendToForwardsPayloadBytes
#   ./run_tests.sh Debug EventBusTest.PublishedEventA_DosNotTriger_EventB
#   ./run_tests.sh Release 'EventBusTest.*'
#
# В качестве имён принимаются имена Google Test вида Suite.TestName.
# Префикс бинарника (например, unit_tests_server.) указывать не требуется —
# скрипт найдёт тест в любом из тестовых бинарников.
# Поддерживаются GTest-wildcard: * (любая последовательность), ? (один символ).
# Имена с wildcard передавайте в кавычках, чтобы их не раскрыла оболочка.

set -e

# ============================================================ аргументы
CONFIG="Debug"
if [ "$#" -gt 0 ] && { [ "$1" = "Debug" ] || [ "$1" = "Release" ]; }; then
    CONFIG="$1"
    shift
fi

if [ "$#" -eq 0 ]; then
    echo "Использование: $0 [Debug|Release] <имя_теста> [<имя_теста> ...]"
    echo
    echo "Примеры:"
    echo "  $0 NetworkResponseRouterTest.SendToForwardsPayloadBytes"
    echo "  $0 Debug EventBusTest.PublishedEventA_DosNotTriger_EventB"
    echo "  $0 Release 'EventBusTest.*'"
    exit 1
fi

TEST_NAMES=("$@")
BUILD_DIR="build/$CONFIG"

# ====================================================== проверка сборки
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Ошибка: сборка '$CONFIG' не сконфигурирована (нет $BUILD_DIR)."
    echo "Сначала выполните:"
    echo "  conan install . --build=missing -s:a build_type=$CONFIG"
    echo "  cmake --preset $(printf '%s' "$CONFIG" | tr '[:upper:]' '[:lower:]')-default"
    exit 1
fi

# ============================================================ функции
# Преобразует имя GTest-теста в регулярное выражение для ctest -R.
# Разделители точек (Suite.Test) экранируются, * и ? остаются wildcard.
# Обёртки (^|\.) и ($|\.) позволяют не указывать префикс бинарника
# и не захватывают тесты с похожими, но более длинными именами.
to_regex() {
    local name="$1" esc
    # Первым делом экранируем сами обратные слэши
    esc=$(printf '%s' "$name" | sed -E 's/\\/\\\\/g')
    # Экранируем метасимволы регулярного выражения
    esc=$(printf '%s' "$esc" | sed -E 's/\./\\\./g; s/\^/\\^/g; s/\$/\\$/g; s/\+/\\+/g; s/\(/\\(/g; s/\)/\\)/g; s/\{/\\{/g; s/\}/\\}/g; s/\[/\\[/g; s/\]/\\]/g; s/\|/\\|/g')
    # GTest-wildcard -> regex
    esc=$(printf '%s' "$esc" | sed -E 's/\*/.*/g; s/\?/./g')
    printf '(^|\\.)%s($|\\.)' "$esc"
}

# ======================================== сборка, если бинарники не найдены
# Бинарники складываются в out/<Config> (см. CMakeLists), поэтому ищем в обоих местах.
need_build=0
for t in unit_tests_common unit_tests_server unit_tests_client; do
    found="$(find "$BUILD_DIR" "out/$CONFIG" -type f -name "$t" -perm -u+x 2>/dev/null | head -n 1)"
    if [ -z "$found" ]; then
        need_build=1
        break
    fi
done

if [ "$need_build" -eq 1 ]; then
    echo "Тестовые бинарники не найдены — собираю таргеты..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" \
        --target unit_tests_common unit_tests_server unit_tests_client
fi

# ======================================================= построение фильтра
REGEX=""
for name in "${TEST_NAMES[@]}"; do
    r="$(to_regex "$name")"
    if [ -n "$REGEX" ]; then
        REGEX="$REGEX|"
    fi
    REGEX="$REGEX$r"
done

echo "Фильтр тестов: $REGEX"

# ========================================================== поиск тестов
LISTING="$(cd "$BUILD_DIR" && ctest -N -C "$CONFIG" -R "$REGEX" 2>&1 || true)"
TOTAL="$(printf '%s\n' "$LISTING" | awk '/Total Tests:/ {print $3; exit}')"

if [ -z "$TOTAL" ] || [ "$TOTAL" -eq 0 ]; then
    echo "По заданным именам тесты не найдены."
    echo
    echo "Доступные тесты (используйте имена вида Suite.TestName):"
    (cd "$BUILD_DIR" && ctest -N -C "$CONFIG")
    exit 2
fi

echo "Найдено тестов: $TOTAL"
printf '%s\n' "$LISTING" | grep -E '^[[:space:]]+Test' || true

echo "================================================================"
echo " Запуск тестов [$CONFIG]..."
echo "================================================================"
(cd "$BUILD_DIR" && ctest -C "$CONFIG" -R "$REGEX" --output-on-failure)

echo "Готово: все запрошенные тесты выполнены."
