Lattice IPC Subsystem
=====================

The **Lattice IPC** subsystem provides capability‐based message passing in XINIM,
leveraging both a low‐latency fastpath for local transfers and a serialized
packet format for distributed operation.

Overview
--------

1. **Local Fastpath** — zero-copy, per-CPU queues with spillover statistics  
2. **Tiered Caching** — L1/L2/L3 buffers to minimize spills  
3. **Distributed Messaging** — encrypted, framed packets over the network  
4. **Directed Graph API** — channel management via a DAG  

Fastpath Overview
-----------------

The fastpath in `kernel/wormhole.hpp` executes a sequence of zero-copy
transforms, yielding directly to the receiver’s thread when the per-CPU queue
can accommodate the message.

- **State**: :cpp:struct:`fastpath::State` holds L1/L2/L3 buffers and counters  
- **Stats**: :cpp:struct:`fastpath::FastpathStats` tracks `hit_count` and `fallback_count`  
- **Entry Point**: :cpp:func:`fastpath::execute_fastpath`  

Cache Hierarchy
---------------

Each `fastpath::State` defines three buffers:

- **L1**: per-core, smallest, lowest latency  
- **L2**: shared among cores on the same socket  
- **L3**: global to the node  

The smallest buffer that fits the message is chosen. If all three are full, the
message “spills” into the shared zero-copy region, configured by
:cpp:func:`fastpath::set_message_region`.

Distributed Operation
---------------------

When channels span multiple nodes, the lattice layer serializes each `message`
into a packet:

1. **Frame**: `[ src_pid | dst_pid | payload_bytes ]`  
2. **Encrypt**: XOR‐stream with the channel’s shared secret  
3. **Transmit**: via :cpp:func:`net::send`  
4. **Receive**: via :cpp:func:`net::recv`  
5. **Reintegrate**: :cpp:func:`lattice::poll_network` decrypts and enqueues  

Graph API
---------

Channels live in a directed acyclic graph, managed by :cpp:class:`lattice::Graph`.

- **Wildcard**: :cpp:var:`lattice::ANY_NODE` for node‐agnostic lookups  
- **Connect**: :cpp:func:`lattice_connect(src, dst, node_id)`  
- **Listen**: :cpp:func:`lattice_listen(pid)`  
- **Send/Recv**: :cpp:func:`lattice_send` / :cpp:func:`lattice_recv`  
- **Poll**: :cpp:func:`lattice::poll_network`  
- **Submit**: :cpp:func:`lattice_channel_submit` / :cpp:func:`lattice_channel_add_dep`  

Remote Channel Setup
--------------------

To connect to a process on another node:

.. code-block:: cpp

   constexpr net::node_t REMOTE = 1;
   constexpr xinim::pid_t SRC_PID = 5;
   constexpr xinim::pid_t DST_PID = 10;

   // Establish a shared‐secret channel SRC_PID → DST_PID on node 1
   int rc = lattice_connect(SRC_PID, DST_PID, REMOTE);
   if (rc != OK) {
       // handle error
   }

Network Driver Behavior
-----------------------

The stubbed network driver in `net_driver.cpp`:

- **Frame** packets with `src_node` and `payload`  
- **Queues** per‐node Packet objects internally  
- **send()** enqueues, **recv()** dequeues for the local node  
- **reset()** clears all queues  

:cpp:func:`net::local_node` hashes `gethostname()` to a stable `node_t`.

Fastpath Integration
--------------------

The wormhole IPC interface in `kernel/wormhole.hpp` implements:

- **State Setup**: allocate zero-copy region  
- **execute_fastpath**: attempts L1→L2→L3 direct transfer, then yields  
- **Scheduler Hook**: on success, immediately switches to the receiver thread  

For full details, consult:

.. doxygenfile:: kernel/wormhole.hpp
   :project: XINIM

.. doxygenfunction:: fastpath::execute_fastpath
   :project: XINIM
