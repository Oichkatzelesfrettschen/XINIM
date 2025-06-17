Lattice IPC Subsystem
=====================

The **Lattice IPC** subsystem provides capability‐based, authenticated, encrypted
message passing in XINIM, combining a **fastpath** for low‐latency local transfers
with a **distributed** packet transport for inter‐node messaging.

Overview
--------
1. **Local Fastpath** — zero‐copy, per‐CPU L1/L2/L3 queues with spillover stats  
2. **Tiered Caching** — L1 (per‐core), L2 (per‐socket), L3 (per‐node), then shared region  
3. **Distributed Messaging** — framed, XOR‐encrypted packets over UDP/TCP  
4. **Directed Graph API** — channels managed in a DAG with cycle‐free dependency tracking  

Fastpath Overview
-----------------
Implemented in `kernel/wormhole.hpp` / `.cpp`:
- **State**: ``fastpath::State`` holds three ring‐buffers and counters  
- **Stats**: ``fastpath::FastpathStats`` tracks `hit_count` vs. `spill_count`  
- **Entry**: :cpp:func:`fastpath::execute_fastpath` attempts L1→L2→L3 transfers, then yields  

Cache Hierarchy
---------------
- **L1 Buffer**: per‐core, minimal latency  
- **L2 Buffer**: shared per‐socket  
- **L3 Buffer**: global per‐node  
- **Spill Region**: shared zero‐copy area via :cpp:func:`fastpath::set_message_region`  

Distributed Operation
---------------------
Each `message` is serialized to:

.. code-block:: text

   [ src_pid | dst_pid | payload_bytes... ]

Pipeline:
1. **Frame**: Prefix with `src_pid` and `dst_pid` (both ints).  
2. **Encrypt**: XOR‐stream with channel’s shared secret.  
3. **Transmit**: :cpp:func:`net::send` (UDP/TCP).  
4. **Receive**: :cpp:func:`net::recv`.  
5. **Reintegrate**: :cpp:func:`lattice::poll_network` decrypts and enqueues.

Network Driver Behavior
-----------------------
Implemented in `net_driver.cpp` / `.hpp`:
- **init(cfg)**: binds UDP socket, TCP listen socket, starts threads  
- **add_remote(node, host, port, proto)**: registers peer (UDP or persistent TCP)  
- **set_recv_callback(cb)**: installs optional callback on packet arrival  
- **send(node, data)**: frames `[local_node|data]`, transmits via UDP or TCP  
- **recv(out_pkt)**: dequeues next `Packet{src_node, payload}`  
- **reset()**: clears internal queue  
- **shutdown()**: stops threads, closes sockets, clears state  

Configuration (`net::Config`):
- ``node_id``: preferred ID (0=auto‐detect)  
- ``port``: UDP/TCP port to bind  
- ``max_queue``: max queued packets  
- ``overflow``: policy (`DROP_OLDEST` | `DROP_NEWEST`)  

Local Node Identification
-------------------------
:cpp:func:`net::local_node` returns the identifier chosen during
:cpp:func:`net::init`.  Detection proceeds in order:
1. configured ``node_id`` if nonzero
2. hash of the primary interface's MAC address
3. hash of the interface's IPv4 address
4. hashed hostname

Graph API
---------
Channels reside in a DAG managed by :cpp:class:`lattice::Graph`:
- **ANY_NODE**: wildcard for node‐agnostic find  
- **lattice_connect(src,dst,node_id)** → OK / error  
- **lattice_listen(pid)**  
- **lattice_send(src,dst,msg,flags)**  
- **lattice_recv(pid,&msg,flags)**  
- **lattice_channel_add_dep(parent,child)**  
- **lattice_channel_submit(chan)**  
- **lattice::poll_network()** integrates remote messages  

Remote Channel Setup
--------------------
.. code-block:: cpp

   constexpr net::node_t REMOTE = 1;
   constexpr pid_t SRC = 5, DST = 10;
   int rc = lattice_connect(SRC, DST, REMOTE);
   if (rc != OK) { /* handle error */ }

Key Exchange: stubbed or real post‐quantum (e.g., Kyber), deriving an XOR key.

Fastpath Integration
--------------------
The fastpath yields directly to the receiver thread via the scheduler hook in
:cpp:func:`fastpath::execute_fastpath`. See `kernel/wormhole.hpp` for details.

Security & Integrity
--------------------
- **Confidentiality**: XOR‐stream with PQ‐derived shared secret  
- **Authentication**: sequence counters + per‐message HMAC tokens  
- **Thread‐safety**: quaternion spinlock guards channel state; DAG prevents deadlock  

For full implementation and API reference, see:
- `kernel/lattice_ipc.hpp` / `.cpp`
- `kernel/wormhole.hpp` / `.cpp`
- `kernel/net_driver.hpp` / `.cpp`
