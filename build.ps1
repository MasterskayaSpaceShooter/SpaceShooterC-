# Скрипт сборки с очисткой кэша для Windows (PowerShell)
param (
    [Parameter(Mandatory=$true)]
    [ValidateSet("server", "client", "tests")]
    [string]$Component,

    [Parameter(Mandatory=$true)]
    [ValidateSet("Debug", "Release")]
    [string]$Config
)

# Маппинг имен компонентов на таргеты CMakeLists.txt
$Target = ""
switch ($Component) {
    "server" { $Target = "server_app" }
    "client" { $Target = "client_app" }
    "tests"  { $Target = "unit_tests" }
}

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Настройка окружения Conan и проверка Ninja..."
Write-Host "=========================================================" -ForegroundColor Cyan

# Проверяем и активируем мультиконфигурационное окружение Конана
$ConanEnvScript = "build/Release/generators/conanbuild.ps1"
if (Test-Path $ConanEnvScript) {
    . ./$ConanEnvScript
} else {
    Write-Error "Ошибка: Скрипты генераторов не найдены. Запустите сначала 'conan install .'"
    exit 1
}

# Если CMake проект ещё не сконфигурирован, запускаем первоначальную генерацию
if (-not (Test-Path "build") -or -not (Test-Path "build/build-Debug.ninja")) {
    Write-Host "Первоначальная конфигурация CMake через пресеты..." -ForegroundColor Yellow
    cmake --preset conan-default
}

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Очистка кэша сборки для таргета: $Target [$Config]"
Write-Host "=========================================================" -ForegroundColor Cyan

# Сбрасываем файлы сборки только для выбранного таргета
cmake --build --preset conan-default --config $Config --target "$Target/clean"

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Сборка таргета: $Target [$Config]"
Write-Host "=========================================================" -ForegroundColor Cyan

# Запускаем сборку
cmake --build --preset conan-default --config $Config --target $Target

# Если пересобирали тесты — сразу их прогоняем через корректный пресет CTest
if ($Component -eq "tests") {
    Write-Host "=========================================================" -ForegroundColor Cyan
    Write-Host " Запуск модульных тестов через Google Test..."
    Write-Host "=========================================================" -ForegroundColor Cyan

    # Динамически собираем имя пресета (conan-debug или conan-release)
    $TestPreset = "conan-$($Config.ToLower())"
    ctest --preset $TestPreset --output-on-failure
}

Write-Host "Успешно завершено! Бинарник находится в out/$Config/" -ForegroundColor Green
