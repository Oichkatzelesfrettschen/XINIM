# XINIM OS Inter-Process Communication (IPC) Subsystem

This directory contains the core components of the XINIM OS Inter-Process Communication (IPC) subsystem, known as Lattice IPC.

## Overview

Lattice IPC is designed to provide a flexible and robust communication mechanism between processes (and potentially kernel modules) within XINIM OS. It aims to support various communication patterns, including message passing and potentially capability-based interactions.

The system is centered around "channels," which are communication endpoints that processes can create, connect to, listen on, and use for sending/receiving data.

## Key Components

*   **`include/public/lattice_api.hpp`**: This header file defines the public Application Programming Interface (API) for Lattice IPC. User-space processes and kernel components will use these functions to interact with the IPC system. It includes functions for:
    *   Connecting to services (`lattice_connect`)
    *   Creating listening endpoints (`lattice_listen`)
    *   Accepting connections (`lattice_accept`)
    *   Sending data (`lattice_send`)
    *   Receiving data (`lattice_recv`)
    *   Closing channels (`lattice_close`)
    It also defines core types like `LatticeHandle` and various flags for controlling IPC behavior.

*   **`include/internal/channel_types.hpp`**: This internal header defines core data structures used by the IPC subsystem, such as:
    *   `struct ChannelCapabilityToken`: A structure intended to hold security and identification information for channel endpoints, utilizing an `XINIM::Algebraic::Octonion`.
    *   `struct Message`: A conceptual representation of a message unit.
    *   `struct Channel`: The primary internal structure representing a communication channel, its state, queues (conceptual), and associated metadata.

*   **`lattice_ipc.cpp`**: This source file contains the implementations of the functions defined in `lattice_api.hpp`. Initially, these are stub implementations that log calls and return default error values. Over time, this file will house the core logic for channel management, message queuing, and IPC operations.

*   **`ALGEBRAIC/` (subdirectory)**: This subdirectory contains the `math_algebras` library, which provides hypercomplex number implementations (Quaternions, Octonions, Sedenions). These algebraic types are primarily intended for:
    *   `ChannelCapabilityToken` (using Octonions).
    *   Experimental security features (e.g., Sedenion-based concepts).
    *   Potentially for other kernel systems requiring these mathematical tools.
    The IPC core library (`xinim_ipc_core`) links against `math_algebras`.

## Design Goals (Conceptual)

*   **Security**: Incorporate capability-based concepts using algebraic types (e.g., Octonions) for fine-grained access control and channel identification.
*   **Flexibility**: Support various communication patterns (e.g., request-response, publish-subscribe eventually).
*   **Performance**: Optimize for low-latency and high-throughput communication, especially for critical system services.
*   **Reliability**: Ensure message delivery guarantees as appropriate for different service types.

## Current Status

*   The public API (`lattice_api.hpp`) and internal data structures (`channel_types.hpp`) have been defined.
*   Stub implementations for the API functions are present in `lattice_ipc.cpp`.
*   The `math_algebras` library (Quaternion, Octonion, Sedenion) is available and linked.
*   CMake build system integration for the IPC core and algebraic libraries is in place.

## Future Work

*   Implement the full logic for all API functions in `lattice_ipc.cpp`.
*   Develop robust kernel data structures for message queues and channel management.
*   Implement synchronization mechanisms (spinlocks, wait queues) for protecting shared IPC data.
*   Integrate with process management for tracking channel ownership and lifecycle.
*   Design and implement the details of the `ChannelCapabilityToken` system and its use in securing IPC.
*   Develop system services that utilize Lattice IPC (e.g., file system server, network server).
*   Add comprehensive unit and integration tests.
