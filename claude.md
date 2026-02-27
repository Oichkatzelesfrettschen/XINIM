# Xinim AI Notes

This repository follows the rules in docs/AGENTS.md. Key points:
- C++23 only; Clang 18+ preferred.
- All modified C++ files require Doxygen comments.
- Run clang-format on modified C++ sources before commit.
- Prefer CMake + Conan; legacy xmake references must be removed or archived.
- Treat warnings as errors.

Operational reminders:
- Keep build instructions in docs/BUILD.md and docs/REQUIREMENTS.md aligned.
- Capture QEMU boot logs under logs/ when validating.
- Use rg/rg --files for fast searches.
