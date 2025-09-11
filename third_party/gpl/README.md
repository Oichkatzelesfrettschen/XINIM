# GPL Components Isolation

This directory contains all GPL-licensed components used by XINIM for compatibility testing and development purposes only.

## ⚠️ LICENSE ISOLATION NOTICE

**All components in this directory are licensed under GPL and are kept strictly isolated from XINIM's main codebase.**

## Components

### 1. POSIX Test Suite (`posixtestsuite-main/`)
- **License**: GPL v2
- **Purpose**: Official POSIX conformance testing
- **Usage**: Testing XINIM's POSIX compliance only
- **Isolation**: Not linked into XINIM binaries

### 2. GXemul Source (`gxemul_source/`)
- **License**: GPL v2 
- **Purpose**: MIPS emulation reference (historical)
- **Usage**: Development reference only
- **Isolation**: Not compiled or linked

## License Compliance

XINIM maintains strict license separation:

1. **Core XINIM**: Pure BSD 2-Clause License
2. **GPL Components**: Isolated in `third_party/gpl/`
3. **No Linking**: GPL code never linked into XINIM binaries
4. **No Derivation**: XINIM code independent of GPL implementations

## Build System Integration

The XINIM build system automatically excludes this directory:

- CMake: `exclude(third_party/gpl/**)`
- xmake: `remove_files("third_party/gpl/**/*")`  
- Native Build: Skips `third_party/gpl/`

## Testing Usage

GPL components used solely for:
- ✅ Conformance testing (external process)
- ✅ Reference documentation
- ✅ Compatibility validation
- ❌ Code generation or derivation
- ❌ Static or dynamic linking
- ❌ Header inclusion

## Legal Compliance

This isolation ensures:
- XINIM remains pure BSD licensed
- GPL components available for testing
- No license contamination
- Clear separation of concerns
- Compliance with both BSD and GPL terms

---

**For questions about license compliance, contact the XINIM legal team.**