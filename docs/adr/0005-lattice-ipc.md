# ADR 0005: Lattice-Based IPC

Status: Proposed

## Context

Traditional microkernel IPC (seL4-style synchronous rendezvous, Mach ports) provides
strong isolation but requires capability management infrastructure. Xinim's novel
approach uses lattice-based IPC where capability levels form a lattice order, enabling
formal security proofs about information flow.

## Decision

Implement IPC via `src/kernel/lattice_ipc.cpp`. Messages carry capability tokens
ordered by a lattice. A send is allowed only if sender's capability dominates receiver's.
The implementation uses a spinlock-protected message queue per process.

## Consequences

- `src/kernel/lattice_ipc.hpp` defines the message types and lattice ordering.
- Server processes (VFS, PM, MM) receive messages via this IPC path.
- Currently: Ring 0 only; Ring 3 migration is deferred to Phase 7+.
- Post-quantum crypto (Kyber) can be layered on IPC channels for secure channels.

## Date

2026-02-26
