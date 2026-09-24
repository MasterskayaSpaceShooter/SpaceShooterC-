# Скрипт сборки с очисткой кэша для Windows (PowerShell)
param (
    [Parameter(Mandatory=$true)]
    [ValidateSet("server", "client", "tests")]
    [string]$Component,

    [Parameter(Mandatory=$true)]
    [ValidateSet("Debug", "Release")]
    [string]$Config
)

# Формируем массив таргетов для сборки
$Targets = @()
switch ($Component) {
    "server" { $Targets = @("server_app") }
    "client" { $Targets = @("client_app") }
    "tests"  { $Targets = @("unit_tests_common", "unit_tests_server", "unit_tests_client") }
}

# КАЖДАЯ конфигурация использует собственную папку сборки (build/Debug, build/Release),
# как в пресетах Конана. Conan раскладывает данные зависимостей (GTest и др.) по
# конфигурациям, поэтому CMake должен читать генераторы именно своей конфигурации.
# Иначе (одна общая папка build/) для Debug-сборки подхватываются только
# Release-данные GTest и include-пути оказываются пустыми.
$BuildDir = "build/$Config"
$GeneratorsDir = "$BuildDir/generators"

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Настройка окружения Conan [$Config]..."
Write-Host "=========================================================" -ForegroundColor Cyan

# Проверяем и активируем окружение Конана, соответствующее конфигурации
$ConanEnvScript = "$GeneratorsDir/conanbuild.ps1"
if (Test-Path $ConanEnvScript) {
    . ./$ConanEnvScript
} else {
    Write-Error "Ошибка: Скрипты генераторов не найдены ($GeneratorsDir). Запустите сначала: conan install . --build=missing -s:a build_type=$Config"
    exit 1
}

# Однократная конфигурация CMake через тулчейн Конана данной конфигурации
if (-not (Test-Path "$BuildDir/CMakeCache.txt")) {
    Write-Host "Конфигурация CMake [$Config]..." -ForegroundColor Yellow
    cmake -B $BuildDir -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE="$GeneratorsDir/conan_toolchain.cmake"
}

# Проходим циклом по всем целям (для раздельной очистки и сборки)
foreach ($Target in $Targets) {
    Write-Host "=========================================================" -ForegroundColor Cyan
    Write-Host " Очистка кэша сборки для таргета: $Target [$Config]"
    Write-Host "=========================================================" -ForegroundColor Cyan
    # Ninja Multi-Config: внутри build/<Config> лежат правила ВСЕХ конфигураций,
    # а дефолтный phony-таргет (build.ninja) указывает на Debug-выходы.
    # Очистка через дефолтный файл удалила бы артефакты другой конфигурации,
    # поэтому чистим ИМЕННО свой конфигурационный файл build-<Config>.ninja.
    ninja -C $BuildDir -f "build-$Config.ninja" -t clean $Target

    Write-Host "=========================================================" -ForegroundColor Cyan
    Write-Host " Сборка таргета: $Target [$Config]"
    Write-Host "=========================================================" -ForegroundColor Cyan
    cmake --build $BuildDir --config $Config --target $Target
}

# Если пересобирали тесты — сразу их прогоняем через CTest
if ($Component -eq "tests") {
    Write-Host "=========================================================" -ForegroundColor Cyan
    Write-Host " Запуск всех модульных тестов через Google Test..."
    Write-Host "=========================================================" -ForegroundColor Cyan
    Push-Location $BuildDir
    ctest -C $Config --output-on-failure
    Pop-Location
}

Write-Host "Успешно завершено! Бинарники находятся в out/$Config/" -ForegroundColor Green
