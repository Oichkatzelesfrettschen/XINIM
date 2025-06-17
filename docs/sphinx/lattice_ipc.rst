Lattice IPC Subsystem
=====================

The **Lattice IPC** subsystem provides capability‐based, authenticated, encrypted
message passing in XINIM, combining a **fastpath** for low‐latency local transfers
with a **distributed** packet transport for inter‐node messaging.

Overview
--------
- **Local Fastpath**: zero‐copy, per‐CPU L1/L2/L3 queues with spillover statistics  
- **Tiered Caching**: L1 (per‐core), L2 (per‐socket), L3 (per‐node), then shared region  
- **Distributed Messaging**: framed, XOR‐encrypted packets over UDP/TCP  
- **Directed Graph API**: channels managed in a DAG with cycle‐free dependency tracking  

Fastpath Overview
-----------------
Implemented in `kernel/wormhole.hpp` / `.cpp`:
- **State**: `fastpath::State` holds three ring‐buffers and counters  
- **Stats**: `fastpath::FastpathStats` tracks `hit_count` vs. `spill_count`  
- **Entry**: `fastpath::execute_fastpath()` attempts L1→L2→L3 transfers, then yields  

Cache Hierarchy
---------------
- **L1 Buffer**: per‐core, minimal latency  
- **L2 Buffer**: shared per‐socket  
- **L3 Buffer**: global per‐node  
- **Spill Region**: shared zero‐copy area via `fastpath::set_message_region()`  

Distributed Operation
---------------------
Each `message` is serialized as:

.. code-block:: text

   [ src_pid | dst_pid | payload_bytes... ]

**Pipeline**:
1. **Frame**: prefix with `src_pid` and `dst_pid` (both ints)  
2. **Encrypt**: XOR‐stream with the channel’s shared secret  
3. **Transmit**: `net::send()` (UDP or TCP)  
4. **Receive**: `net::recv()`  
5. **Reintegrate**: `lattice::poll_network()` decrypts and enqueues  

Network Driver Behavior
-----------------------
Implemented in `net_driver.hpp` / `.cpp`:
- **init(cfg)**: bind UDP & TCP sockets, start background I/O threads  
- **add_remote(node, host, port, proto)**: register peer (UDP or persistent TCP)  
- **set_recv_callback(cb)**: optional callback on arrival  
- **send(node, data)**: frame `[local_node|data]`, transmit via UDP/TCP  
- **recv(out_pkt)**: dequeue next `Packet{src_node, payload}`  
- **reset()**: clear internal queue  
- **shutdown()**: stop threads, close sockets, clear state  

Configuration (`net::Config`):
- ``node_id``: preferred ID (0=auto‐detect)  
- ``port``: UDP/TCP port to bind  
- ``max_queue``: max queued packets  
- ``overflow``: policy (`DROP_OLDEST` | `DROP_NEWEST`)

Network Driver
--------------
The networking backend transports packets over UDP or TCP. Calling
:cpp:func:`net::init` binds the UDP socket and spawns background threads for
receives and TCP connection handling. Each remote node registers its address
with :cpp:func:`net::add_remote`. Frames are transmitted by
:cpp:func:`net::send`. For TCP peers the function establishes a transient
connection when necessary. Incoming datagrams remain queued until
:cpp:func:`lattice::poll_network` decrypts and enqueues them for IPC.

Example
^^^^^^^
.. code-block:: cpp

   net::init({0, 15000});
   net::add_remote(1, "127.0.0.1", 15001);
   lattice_connect(1, 1, 1);

   message ping{};
   ping.m_type = 42;
   lattice_send(1, 1, ping);

   for (;;) {
       lattice::poll_network();
       if (lattice_recv(1, &ping) == OK) {
           break;
       }
   }
   net::shutdown();

Local Node Identification
-------------------------
The function ``net::local_node()`` returns, in order:
1. the configured ``node_id`` if nonzero
2. a hash of the first active, non-loopback network interface (MAC or IPv4)
3. a fallback hash of the local hostname

Graph API
---------
Channels live in a DAG managed by `lattice::Graph`:
- **ANY_NODE**: wildcard for node‐agnostic lookup  
- `lattice_connect(src, dst, node_id)` → OK / error  
- `lattice_listen(pid)`  
- `lattice_send(src, dst, msg, flags)`  
- `lattice_recv(pid, &msg, flags)`  
- `lattice_channel_add_dep(parent, child)`  
- `lattice_channel_submit(chan)`  
- `lattice::poll_network()` integrates remote packets  

Remote Channel Setup
--------------------
.. code-block:: cpp

   constexpr net::node_t REMOTE = 1;
   constexpr pid_t SRC = 5, DST = 10;

   int rc = lattice_connect(SRC, DST, REMOTE);
   if (rc != OK) {
       // handle error
   }

Key Exchange
------------
Uses stubbed or real post‐quantum (e.g., Kyber) key exchange to derive an
XOR‐stream secret for encryption/decryption.

Security & Integrity
-------------------
- **Confidentiality**: XOR‐stream with PQ‐derived shared secret  
- **Authentication**: sequence counters + per‐message HMAC tokens  
- **Thread‐safety**: quaternion spinlock guards channel state; DAG prevents deadlock  

See Also
--------
- `kernel/lattice_ipc.hpp` / `.cpp`  
- `kernel/wormhole.hpp` / `.cpp`  
- `kernel/net_driver.hpp` / `.cpp`  
- `kernel/schedule.hpp` / `.cpp`  
