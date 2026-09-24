# Автономная и кроссплатформенная конфигурация. 

Она работает идентично на Windows, Linux и macOS. Менеджер пакетов Conan сам скачает компилятор/сборщик Ninja, библиотеку Boost целиком, а также тестовый фреймворк Google Test. Фиксирует версии пакетов для всех участников разработки. Скомпилированные таргеты компилируются и собираются в папку `./out`.

## Установка.
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
```bash
conan install . --build=missing -s:a build_type=Debug -s:a build_type=Release
```

### 5. Активация виртуального окружения Conan (Чтобы ОС увидела внутренний Ninja)

- Для Windows
```bash
. ./build/Release/generators/conanbuild.ps1
```

- Для Linux / macOS
```bash
source build/Release/generators/conanbuild.sh
```

### 6. Однократное конфигурирование проекта через CMake
```bash
cmake --preset conan-default
```

### 7. Раздельная компиляция таргетов (Сервер, Клиент, Тесты)
```bash
# Сборка СЕРВЕРА
cmake --build --preset conan-default --config Debug --target server_app
cmake --build --preset conan-default --config Release --target server_app

# Сборка КЛИЕНТА
cmake --build --preset conan-default --config Debug --target client_app

# Сборка и запуск ТЕСТОВ
cmake --build --preset conan-default --config Debug --target unit_tests
```

### 8. Настройка запуска тестов через CTest
```bash
ctest --preset conan-debug
ctest --preset conan-release
```

## Запуск сборок.
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
