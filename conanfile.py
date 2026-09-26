from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

class NetworkProjectRecipe(ConanFile):
    name = "network_project"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.86.0")
        # GTest — обычная (host) зависимость: тесты линкуются с её библиотеками.
        # Если объявить её tool_requires, Conan поместит пакет в build-контекст
        # и CMakeDeps не заполнит include/lib информацию (пустые gtest_INCLUDE_DIRS).
        self.requires("gtest/1.15.0")

    def build_requirements(self):
        # Только инструменты сборки: автономный Ninja
        self.tool_requires("ninja/1.12.1")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja Multi-Config"

        bt = str(self.settings.build_type).lower()
        # Уникальные имена пресетов для каждой конфигурации. Conan по умолчанию
        # генерирует в обоих файлах конфигур-пресет 'conan-default', а CMake
        # падает с ошибкой 'Duplicate preset' при слиянии пресетов. Префикс даёт
        # имена debug-default / release-default и устраняет конфликт.
        tc.presets_prefix = bt
        # Сгенерированный CMakePresets.json подключается из корневого
        # CMakeUserPresets.json, а относительные пути (toolchainFile) CMake
        # резолвит относительно корня проекта — нужны абсолютные пути.
        tc.absolute_paths = True
        # user_presets_path оставлен по умолчанию ("CMakeUserPresets.json"):
        # Conan 2.32 сам добавляет include текущей конфигурации в единый
        # корневой файл, объединяя Debug и Release (удалённые папки вычищает).

        tc.generate()

        # gtest теперь обычная (host) зависимость — build-контекст не требуется,
        # иначе CMakeDeps генерирует таргеты GTest::* без include/библиотек
        deps = CMakeDeps(self)
        deps.generate()



    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
