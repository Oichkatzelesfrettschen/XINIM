# MINIX Boot Image Builder

A modern C++17 implementation of the MINIX boot image builder tool. This utility combines bootblock, kernel, and system process executables into a single bootable disk image.

## Features

- **Modern C++17 Design**: Uses modern C++ idioms, RAII, and strong typing
- **Type Safety**: Strong types prevent parameter confusion and improve compile-time safety
- **Exception Safety**: Comprehensive error handling with descriptive messages
- **Memory Safety**: Automatic resource management with RAII patterns
- **Performance**: Optimized I/O with sector-based buffering
- **Robustness**: Input validation and magic number verification

## Building

### Requirements

- Clang++ with C++17 support
- Make
- clang-format (optional, for code formatting)
- clang-tidy (optional, for linting)

### Build Commands

```bash
# Release build (optimized)
make release

# Debug build (with sanitizers)
make debug

# Clean build artifacts
make clean

# Format code
make format

# Lint code
make lint

# Install to system
sudo make install
```

## Usage

```bash
./build/build bootblock kernel mm fs init fsck output_image
```

### Arguments

- `bootblock`: Boot sector binary (512 bytes max)
- `kernel`: MINIX kernel executable
- `mm`: Memory manager executable
- `fs`: File system executable  
- `init`: Init process executable
- `fsck`: File system checker executable
- `output_image`: Output boot image file path

### Example

```bash
./build/build boot/bootblock kernel/kernel mm/mm fs/fs init/init fsck/fsck minix.img
```

## Architecture

### Key Components

1. **ByteOffset**: Strong type for byte offsets to prevent parameter confusion
2. **BuildConstants**: Compile-time constants for MINIX format specifications
3. **ProgramInfo**: Metadata about executable segments and memory layout
4. **SectorBuffer**: 512-byte sector buffer with automatic alignment
5. **ImageFile**: Boot image file manager with RAII and sector-based I/O
6. **ExecutableHeader**: Parser for MINIX executable file headers
7. **BootImageBuilder**: Main orchestrator for the build process

### Memory Layout

```
Sector 0:    Bootblock (512 bytes)
Sector 1+:   Kernel
             Memory Manager (MM)
             File System (FS)
             Init Process
             File System Checker (FSCK)
```

### Build Process

1. **Copy Bootblock**: Load boot sector into image
2. **Parse Headers**: Extract segment information from executables
3. **Copy Programs**: Load text, data, and BSS segments with alignment
4. **Patch Bootblock**: Update with total size and kernel entry point
5. **Update Kernel Table**: Set process memory layout information
6. **Initialize FS Data**: Provide init process location to file system

## File Format Support

- **32-byte headers**: Standard MINIX executable format
- **48-byte headers**: Extended MINIX executable format
- **Separate I&D**: Support for separate instruction and data spaces
- **Combined I&D**: Support for unified address space programs

## Error Handling

The builder provides comprehensive error reporting for:

- Missing or invalid input files
- Corrupt executable headers
- Alignment requirements violations
- Magic number validation failures
- I/O errors during build process

## Technical Details

### Alignment Requirements

- All programs must be aligned to 16-byte boundaries
- Separate I&D programs require text segment alignment
- BSS segments are automatically padded for alignment

### Magic Numbers

- Kernel data space: `0x526F`
- File system data space: `0xDADA`

### Memory Clicks

MINIX uses 16-byte "clicks" for memory management:

- Text clicks = text_size >> 4 (separate I&D only)
- Data clicks = (data_size + bss_size) >> 4

## Development

### Code Style

The project follows modern C++17 best practices:

- RAII for resource management
- `constexpr` for compile-time constants
- `[[nodiscard]]` for important return values
- `noexcept` specifications where appropriate
- Strong typing to prevent errors
- Comprehensive documentation

### Contributing

1. Ensure code follows the existing style
2. Add appropriate documentation
3. Include error handling for new functionality
4. Test with various executable formats
5. Update this README for new features

## License

This implementation is part of the MINIX project and follows the same licensing terms.