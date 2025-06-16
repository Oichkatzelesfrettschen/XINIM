Lattice IPC Subsystem
=====================

The lattice IPC subsystem provides capability-based message passing in XINIM. It
enables fast communication between threads using the fastpath implementation
found in :file:`kernel/wormhole.hpp`.

Fastpath Overview
-----------------

The fastpath executes a series of transformations that perform zero-copy
transfer and update scheduling state.

.. doxygenfile:: wormhole.hpp
   :project: XINIM

.. doxygenstruct:: fastpath::State
   :project: XINIM

.. doxygenstruct:: fastpath::FastpathStats
   :project: XINIM

The statistics track how often the per-CPU fastpath queue succeeds as
well as the number of spill events when the queue is exhausted.  The
``hit_count`` and ``fallback_count`` fields provide this insight.

.. doxygenfunction:: fastpath::execute_fastpath
   :project: XINIM

Cache Hierarchy
---------------

The fastpath employs a tiered cache system to avoid spilling messages to the
shared zero-copy buffer.  Each :cpp:struct:`fastpath::State` defines three
caches named ``l1_buffer``, ``l2_buffer`` and ``l3_buffer``.  During
``execute_fastpath`` the smallest cache capable of holding the message is chosen
automatically.  Only when all caches are exhausted are the registers copied
through the main region configured with
:cpp:func:`fastpath::set_message_region`.

Distributed Operation
---------------------

When nodes are connected over a network the lattice layer serializes messages
into a small packet structure.  The packet begins with the sending and
receiving process identifiers followed by the raw message bytes.  Packets are
transmitted through :cpp:func:`net::send` and recovered with
:cpp:func:`net::recv`.

The helper :cpp:func:`lattice::poll_network` converts incoming packets back into
messages.  Each message is encrypted with the channel's shared secret before
being queued.  Applications call this function periodically to integrate remote
messages into the standard queueing mechanism.

Graph API
---------

Channels are stored in a directed acyclic graph accessible through
:cpp:class:`lattice::Graph`. The helper constant
:cpp:var:`lattice::ANY_NODE` selects a wildcard search across all nodes when
looking up connections.

.. doxygenclass:: lattice::Graph
   :project: XINIM
   :members:

.. doxygenvariable:: lattice::ANY_NODE
   :project: XINIM

Remote Channel Setup
--------------------

The lattice IPC layer supports connecting processes across multiple nodes.
Use :cpp:func:`lattice_connect` with a non-zero ``node_id`` so the network
driver can exchange handshake packets. The driver synchronizes channel
metadata before capability tokens are installed.

Example connection to a remote node:

.. code-block:: cpp

   constexpr net::node_t REMOTE = 1;
   constexpr xinim::pid_t SRC_PID = 5;
   constexpr xinim::pid_t DST_PID = 10;

   // Establish a channel from SRC_PID on this node to DST_PID on node 1
   lattice_connect(SRC_PID, DST_PID, REMOTE);

Fastpath Integration
--------------------

The wormhole IPC interface is declared in :file:`kernel/wormhole.hpp`. It
defines the *State* data structure along with helper utilities that prepare the
zero-copy message region.  :file:`kernel/wormhole.cpp` implements the
transformation steps that move messages, manage endpoint queues and invoke the
scheduler.

When threads exchange messages successfully, control transfers to the receiver
through the global scheduler.  The integration point is documented in
``fastpath::execute_fastpath`` which yields to the destination thread.

.. doxygenfunction:: fastpath::execute_fastpath
   :project: XINIM
