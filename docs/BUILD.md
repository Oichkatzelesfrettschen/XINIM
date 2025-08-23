# Build Instructions

## Prerequisites
- CMake 3.5 or newer
- Clang 18 toolchain and LLVM utilities (`lld`, `lldb`)
- Doxygen for API documentation

## Configure
```bash
cmake -S . -B build
```

## Compile
```bash
cmake --build build
```

## Generate Documentation
```bash
cmake --build build --target doc
```

The documentation target emits HTML and XML into `docs/doxygen`,
ready for consumption by Sphinx via the Breathe extension.
