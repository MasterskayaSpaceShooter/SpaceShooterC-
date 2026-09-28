# Автономная и кроссплатформенная конфигурация

Она работает идентично на Windows, Linux и macOS. Менеджер пакетов Conan сам скачает компилятор/сборщик Ninja, библиотеку Boost целиком, а также тестовый фреймворк Google Test. Фиксирует версии пакетов для всех участников разработки. Исполняемые файлы и библиотеки собираются в папку `./out`.

## Установка

Установку производим из корневой директории проекта.

### 1. Менеджер пакетов Conan 2.32

- Установка под Windows.

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

`build/` и `out/` — артефакты сборок; `CMakeUserPresets.json` — единый файл
пресетов, который автоматически пересоздаётся на шаге 4, поэтому его удаление
безопасно.

### 4. Атомарная установка зависимостей Конана для режимов DEBUG и RELEASE

ВНИМАНИЕ: установку нужно выполнять ПО ОТДЕЛЬНОСТИ для каждой конфигурации —
одна команда с двумя `-s:a build_type=...` не работает (побеждает последнее значение).

```bash
conan install . --build=missing -s:a build_type=Debug
conan install . --build=missing -s:a build_type=Release
```

Каждая конфигурация получит собственную папку генераторов:
`build/Debug/generators` и `build/Release/generators`.

Обе команды дополняют ЕДИНЫЙ корневой файл пресетов `CMakeUserPresets.json`:
Conan добавляет в него include своей конфигурации и вычищает записи удалённых
папок. Порядок установки не важен, повторные запуски идемпотентны — дубликаты
включаемых путей не появляются. После изменения `conanfile.py` достаточно
повторно выполнить эти две команды: тулчейны и пресеты обновятся автоматически.

### 5. Активация виртуального окружения Conan (Чтобы ОС увидела внутренний Ninja)

- Для Windows

```bash
. ./build/Release/generators/conanbuild.ps1
```

- Для Linux / macOS

```bash
source build/Release/generators/conanbuild.sh
```

### 6. Сборка проекта скриптами (конфигурации и тесты)

Проект конфигурируется и собирается скриптами [`build.sh`](build.sh)
(Linux/macOS) и [`build.ps1`](build.ps1) (Windows). Скрипт автоматически:

- активирует окружение Conan нужной конфигурации (внутренний Ninja);
- конфигурирует проект через пресет `<config>-default`, если `CMakeCache.txt`
  в `build/<Config>/` ещё нет (однократно);
- очищает и собирает указанные таргеты;
- для `tests` дополнительно запускает все модульные тесты через CTest.

Команды для каждого компонента (сервер, клиент, тесты) и каждой конфигурации
(Debug, Release):

- Для Linux / macOS

```bash
./build.sh server Debug      # Сборка сервера (Debug)
./build.sh server Release    # Сборка сервера (Release)
./build.sh client Debug      # Сборка клиента (Debug)
./build.sh client Release    # Сборка клиента (Release)
./build.sh tests Debug       # Сборка и запуск тестов (Debug)
./build.sh tests Release     # Сборка и запуск тестов (Release)
```

- Для Windows (PowerShell)

```powershell
.\build.ps1 -Component server -Config Debug    # Сборка сервера (Debug)
.\build.ps1 -Component server -Config Release  # Сборка сервера (Release)
.\build.ps1 -Component client -Config Debug    # Сборка клиента (Debug)
.\build.ps1 -Component client -Config Release  # Сборка клиента (Release)
.\build.ps1 -Component tests -Config Debug     # Сборка и запуск тестов (Debug)
.\build.ps1 -Component tests -Config Release   # Сборка и запуск тестов (Release)
```

Собранные исполняемые файлы и библиотеки попадают в `out/<Config>/`
(`out/Debug/`, `out/Release/`).

> **Примечание. Конфигурирование и сборка вручную (без скриптов).**
>
> Скрипты лишь оборачивают обычные команды CMake. Пресеты `debug-default` /
> `release-default` генерирует Conan на шаге 4 в едином корневом файле
> `CMakeUserPresets.json`; каждый пресет сам задаёт генератор Ninja Multi-Config,
> бинарную папку `build/<Config>` и тулчейн
> `build/<Config>/generators/conan_toolchain.cmake`. Вручную это делается так:
>
> ```bash
> cmake --preset debug-default      # Конфигурирование build/Debug
> cmake --preset release-default    # Конфигурирование build/Release
> cmake --build build/Debug --config Debug --target server_app
> cmake --build build/Release --config Release --target client_app
> ```
>
> Пресеты появляются только ПОСЛЕ `conan install` (шаг 4): на свежем клоне
> сначала выполните установку зависимостей, иначе `cmake --preset` сообщит,
> что пресет не найден. Проверить доступные пресеты можно командой
> `cmake --list-presets`. IDE (VS Code, CLion, Visual Studio) подхватывают
> `CMakeUserPresets.json` автоматически при открытии корня проекта.
