# ADR 0007: Network Driver Architecture

**Date**: 2026-02-26
**Status**: Accepted
**Deciders**: Xinim development team

## Context

The current network driver (`src/kernel/net_driver.cpp`) is a stub that returns
failure for all operations. The interface (`include/xinim/net/net_driver.hpp`)
defines `NetDriver` with `init/send/recv/shutdown` and a `local_node()` function.

Lattice IPC uses `net_driver.hpp` for remote-node message routing. The local
node ID is hardcoded to 1.

## Decision

**Use virtio-net as the Phase 7 network driver target.**

Rationale:
- QEMU provides virtio-net with no physical hardware required.
- virtio-net is well-documented (virtio 1.1 spec, section 5.1).
- Avoids PCI device probing complexity in early phases.
- The existing `NetDriver` stub interface maps cleanly to virtio-net operations.

## Implementation Plan (Phase 7)

1. Add `src/drivers/net/virtio_net.cpp` implementing `NetDriver` via virtio-mmio.
2. Wire virtio_net into `net_driver.cpp` init path behind a compile-time flag.
3. Remove `local_node() == 1` stub; read actual node ID from PCI config or QEMU.
4. Update lattice IPC to use real network path for remote messages.

## Stub Interface Contract (Current)

The stub satisfies:
- `local_node()` returns 1 (single-node operation)
- `send()` returns false (no packet sent)
- `recv()` returns false (no packet available)
- `init()` returns false (no hardware driver)
- `shutdown()` is a no-op

These semantics are tested in `test/test_net_driver_stub.cpp`.

## Consequences

- Phase 7 begins virtio-net driver in `src/drivers/net/`
- The `NetDriver` interface in `include/xinim/net/net_driver.hpp` is the stable ABI
- Stub tests in `test_net_driver_stub.cpp` will be superseded by integration tests
