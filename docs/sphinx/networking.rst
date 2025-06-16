Networking Driver
=================

The networking layer transports lattice IPC packets between nodes using either
UDP datagrams or TCP streams.  A node binds to a local UDP port and registers
remote peers through :cpp:func:`net::add_remote` selecting the desired
protocol.  The driver spawns a background thread to poll the UDP socket and
invokes an optional callback whenever a packet arrives. TCP connections are
established lazily when the remote is added and reused for subsequent
transmissions.

API Overview
------------

.. doxygenstruct:: net::Config
   :project: XINIM

.. doxygentypedef:: net::RecvCallback
   :project: XINIM

.. doxygenfunction:: net::init
   :project: XINIM

.. doxygenfunction:: net::add_remote
   :project: XINIM

.. doxygenfunction:: net::set_recv_callback
   :project: XINIM

.. doxygenfunction:: net::send
   :project: XINIM

.. doxygenfunction:: net::local_node
   :project: XINIM

.. doxygenfunction:: net::recv
   :project: XINIM

.. doxygenfunction:: net::shutdown
   :project: XINIM
