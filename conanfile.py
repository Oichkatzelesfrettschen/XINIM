from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import cmake_layout


class XinimConan(ConanFile):
    name = "xinim"
    version = "0.1.0"

    settings = "os", "arch", "compiler", "build_type"

    # No Conan-managed runtime deps: crypto uses vendored sources (src/crypto/vendored_sodium/).
    generators = (
        "CMakeToolchain",
        "CMakeDeps",
    )

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "include/*",
        "userland/*",
        "test/*",
        "third_party/limine/*",
        "scripts/*",
        "docs/*",
    )

    def validate(self):
        if str(self.settings.compiler) != "clang":
            raise ConanInvalidConfiguration("Xinim requires clang as the compiler.")
        try:
            compiler_version = int(str(self.settings.compiler.version))
        except ValueError as exc:
            raise ConanInvalidConfiguration(
                f"Unsupported clang version: {self.settings.compiler.version}"
            ) from exc
        if compiler_version < 18:
            raise ConanInvalidConfiguration("Xinim requires clang >= 18.")
        check_min_cppstd(self, "23")

    def layout(self):
        cmake_layout(self)
