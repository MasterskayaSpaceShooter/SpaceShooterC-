from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

class NetworkProjectRecipe(ConanFile):
    name = "network_project"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("boost/1.86.0")

    def build_requirements(self):
        self.tool_requires("gtest/1.15.0")
        self.tool_requires("ninja/1.12.1") # Автономный Ninja

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja Multi-Config" # Мультиконфигурационный режим
        tc.generate()

        deps = CMakeDeps(self)
        # Принудительно заставляем Конан создать файлы конфигурации (gtest-config.cmake)
        # для инструментов из секции build_requirements!
        deps.build_context_activated = ["gtest"]
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
