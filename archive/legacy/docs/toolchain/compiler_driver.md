<!--
@file compiler_driver.md
@brief Overview of the modern XINIM compiler driver.
@details Describes the `CompilerDriver` class that replaces the legacy
global state, and outlines the dedicated CMake target `minix_cmd_cc`.
-->

# Compiler Driver

The XINIM toolchain now employs an object-oriented compiler driver encapsulated
in the `CompilerDriver` class. This structure consolidates all compilation
options and transient state, eliminating the former global `compiler_state`
struct. The driver coordinates preprocessing, compilation, optimization, code
generation, and linking.

The build system provides an explicit CMake target `minix_cmd_cc` that compiles
`commands/cc.cpp` and links it against the requisite system libraries. This
target is isolated from the generic command builds to keep the compilation
pipeline self-contained.

For API details, refer to the Sphinx documentation generated via Breathe.

