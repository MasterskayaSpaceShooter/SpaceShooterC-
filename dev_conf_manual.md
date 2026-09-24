# Автономная и кроссплатформенная конфигурация

Она работает идентично на Windows, Linux и macOS. Менеджер пакетов Conan сам скачает компилятор/сборщик Ninja, библиотеку Boost целиком, а также тестовый фреймворк Google Test. Фиксирует версии пакетов для всех участников разработки. Скомпилированные таргеты компилируются и собираются в папку `./out`.

## Установка

Установку производим из корневой директории проекта.

### 1. Для разработки исользуем менеджер пакетов Conan 2.32

- Установка под WIndows.

```bash
winget install Conan.Conan --version 2.32.0
```

- Установка под Linux/Mac.

```bash
sudo apt update && sudo apt install pipx -y     # Для Linux

brew install pipx                               # Для macOS

pipx ensurepath # Перезапустите терминал после выполнения ensurepath!
```

```bash
pipx install conan==2.32.0
conan --version # Должно строго вернуть: Conan version 2.32.0
```

### 2. Первоначальная настройка компилятора

```bash
conan profile detect --force
```

### 3. Очистка старых конфликтующих пресетов (если они были)

```bash
rm -rf build/
rm -rf out/
rm -f CMakePresets.json
rm -f CMakeUserPresets.json
```

### 4. Атомарная установка зависимостей Конана для режимов DEBUG и RELEASE

ВНИМАНИЕ: установку нужно выполнять ПО ОТДЕЛЬНОСТИ для каждой конфигурации —
одна команда с двумя `-s:a build_type=...` не работает (побеждает последнее значение).

```bash
conan install . --build=missing -s:a build_type=Debug
conan install . --build=missing -s:a build_type=Release
```

Каждая конфигурация получит собственную папку генераторов:
`build/Debug/generators` и `build/Release/generators`.

### 5. Активация виртуального окружения Conan (Чтобы ОС увидела внутренний Ninja)

- Для Windows

```bash
. ./build/Release/generators/conanbuild.ps1
```

- Для Linux / macOS

```bash
source build/Release/generators/conanbuild.sh
```

### 6. Конфигурирование проекта через CMake (по конфигурациям)

Каждая конфигурация использует СОБСТВЕННУЮ папку сборки со своим тулчейном Конана:
`build/Debug` и `build/Release`. Скрипты `build.sh` / `build.ps1` делают это
автоматически; вручную:

```bash
cmake -B build/Debug  -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE=build/Debug/generators/conan_toolchain.cmake
cmake -B build/Release -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake
```

Почему нельзя конфигурировать один раз в общую папку `build/`: Conan раскладывает
данные зависимостей (GTest и др.) по конфигурациям, и для корректных include/линк
путей CMake должен читать генераторы именно своей конфигурации.

### 7. Раздельная компиляция таргетов (Сервер, Клиент, Тесты)

```bash
# Сборка СЕРВЕРА (Debug)
cmake --build build/Debug --config Debug --target server_app

# Сборка КЛИЕНТА (Release)
cmake --build build/Release --config Release --target client_app

# Сборка и запуск ТЕСТОВ (проще через скрипт — он сам конфигурирует, собирает и гоняет CTest)
./build.sh tests Debug
```

### 8. Запуск тестов через CTest

```bash
cd build/Debug && ctest -C Debug --output-on-failure
cd build/Release && ctest -C Release --output-on-failure
```

## Запуск сборок

Запуск осуществляется путём запуска соответствующих скриптов.

- Для Windows

```bash
# Собрать клиент в релиз
./build.ps1 -Component client -Config Release
```

- Для Linux/Mac

```bash
# Собрать сервер для отладки
./build.sh server Debug

# Собрать и прогнать юнит-тесты
./build.sh tests Debug
```
