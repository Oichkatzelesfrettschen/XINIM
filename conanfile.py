import re
from pathlib import Path

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class XinimConan(ConanFile):
    name = "xinim"
    version = "0.1.0"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "lane": (
            "x86_64",
            "i486",
            "i586",
            "i686",
            "x86_32_core2",
            "x86_32_athlon",
            "x86_32_phenom",
        ),
        "x86_32_toolchain_mode": ("clang-m32", "cross-elf"),
        "vfs_profile": ("auto", "default", "tiny"),
        "x86_elf_toolchain_triple": [None, "ANY"],
        "x86_elf_toolchain_root": [None, "ANY"],
    }
    default_options = {
        "lane": "x86_64",
        "x86_32_toolchain_mode": "clang-m32",
        "vfs_profile": "auto",
        "x86_elf_toolchain_triple": None,
        "x86_elf_toolchain_root": None,
    }

    # No Conan-managed runtime deps: crypto uses vendored sources (src/crypto/vendored_sodium/).

    exports_sources = (
        "boot/*",
        "CMakeLists.txt",
        "cmake/*",
        "linker*.ld",
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

        lane = str(self.options.lane)
        arch = str(self.settings.arch)
        toolchain_mode = str(self.options.x86_32_toolchain_mode)
        if lane == "x86_64" and arch != "x86_64":
            raise ConanInvalidConfiguration(
                "The x86_64 lane requires an x86_64 Conan host profile."
            )
        if lane == "x86_64" and toolchain_mode != "clang-m32":
            raise ConanInvalidConfiguration(
                "x86_32_toolchain_mode only applies to 32-bit lanes."
            )
        if lane != "x86_64" and toolchain_mode == "clang-m32" and arch != "x86":
            raise ConanInvalidConfiguration(
                f"The {lane} lane requires an x86 Conan host profile when "
                "x86_32_toolchain_mode=clang-m32."
            )
        if lane != "x86_64" and toolchain_mode == "cross-elf" and arch not in ("x86", "x86_64"):
            raise ConanInvalidConfiguration(
                f"The {lane} lane with x86_32_toolchain_mode=cross-elf requires "
                "an x86 or x86_64 Conan host profile."
            )

    def layout(self):
        self.folders.build = "."
        self.folders.generators = "generators"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        toolchain = CMakeToolchain(self)
        toolchain.user_presets_path = False
        cross_triple = str(self.options.x86_elf_toolchain_triple)
        cross_root = str(self.options.x86_elf_toolchain_root)
        if cross_triple == "None":
            cross_triple = ""
        if cross_root == "None":
            cross_root = ""
        toolchain.cache_variables["XINIM_CPU_LANE"] = str(self.options.lane)
        toolchain.cache_variables["XINIM_ARTIFACT_ROOT"] = self.build_folder
        toolchain.cache_variables["XINIM_BUILD_ROOT"] = self.build_folder
        toolchain.cache_variables["XINIM_IMAGE_ROOT"] = f"{self.build_folder}/images"
        toolchain.cache_variables["XINIM_LOG_ROOT"] = f"{self.build_folder}/logs"
        toolchain.cache_variables["XINIM_TOOLS_ROOT"] = f"{self.build_folder}/tools"
        toolchain.cache_variables["XINIM_X86_32_TOOLCHAIN_MODE"] = str(
            self.options.x86_32_toolchain_mode
        )
        toolchain.cache_variables["XINIM_VFS_PROFILE"] = str(self.options.vfs_profile)
        toolchain.cache_variables["XINIM_X86_ELF_TOOLCHAIN_TRIPLE"] = cross_triple
        toolchain.cache_variables["XINIM_X86_ELF_TOOLCHAIN_ROOT"] = cross_root
        toolchain.generate()
        if str(self.options.x86_32_toolchain_mode) == "cross-elf":
            self._rewrite_cross_elf_toolchain()

    def _rewrite_cross_elf_toolchain(self):
        toolchain_path = Path(self.generators_folder) / "conan_toolchain.cmake"
        toolchain_text = toolchain_path.read_text(encoding="utf-8")
        block_patterns = (
            (
                "compilers",
                "Conan compiler selection is disabled for XINIM cross-elf mode.\n"
                "# The active 32-bit lane selects its ELF cross compiler in CMakeLists.txt before project().\n",
            ),
            (
                "arch_flags",
                "Conan host architecture flags are disabled for XINIM cross-elf mode.\n"
                "# The active 32-bit lane provides its own freestanding target flags.\n",
            ),
            (
                "libcxx",
                "Conan libcxx flags are disabled for XINIM cross-elf mode.\n"
                "# Bare-metal ELF cross compilers do not consume host C++ runtime selection flags.\n",
            ),
        )
        for block_name, replacement_body in block_patterns:
            pattern = (
                r"########## '%s' block #############\n"
                r".*?"
                r"(?=\n########## ')" % re.escape(block_name)
            )
            replacement = (
                "########## '%s' block #############\n"
                "# %s" % (block_name, replacement_body)
            )
            toolchain_text, count = re.subn(
                pattern,
                replacement,
                toolchain_text,
                count=1,
                flags=re.S,
            )
            if count != 1:
                raise ConanInvalidConfiguration(
                    f"Unable to rewrite the Conan '{block_name}' block for cross-elf mode."
                )
        toolchain_path.write_text(toolchain_text, encoding="utf-8")
