#pragma once
// PGO profile dump for bare-metal i486 kernel.
// WHY: libclang_rt.profile writes profiles to files; we have no filesystem
// at shutdown, so we serialize the profile buffer to COM1 serial instead.
// The host-side scripts/extract_pgo_profile.py recovers it from the VBox log.
// Only compiled when XINIM_PGO_DUMP_ENABLED=1 (XINIM_PGO_MODE=generate).

#ifdef XINIM_PGO_DUMP_ENABLED

namespace xinim::i486::pgo {

// Write the LLVM instrumentation profile to COM1 serial.
// Format: XNPGO_START:<hex8_size>\n<hex_pairs>\nXNPGO_END\n
// Call once at kernel shutdown before powering off.
void dump_profile_via_serial() noexcept;

// Provide weak stubs for libc symbols pulled in by libclang_rt.profile-i386.a.
// These are defined in pgo_dump.cpp and declared here so the linker sees them.
// None of the stub'd paths are exercised; only write_buffer is called.

} // namespace xinim::i486::pgo

#endif // XINIM_PGO_DUMP_ENABLED
