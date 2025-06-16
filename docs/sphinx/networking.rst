Networking Driver
=================

The UDP networking layer transports lattice IPC packets between nodes.  A node
binds to a local UDP port and registers remote peers through
:cpp:func:`net::add_remote`.  The driver spawns a background thread to poll the
socket and invokes an optional callback whenever a packet arrives.

When ``Config::node_id`` is set to ``0`` the driver derives a unique identifier
by hashing the primary network interface.  This automatically detected value is
returned by :cpp:func:`net::local_node` unless a different identifier is
provided in the configuration.

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

.. doxygenfunction:: net::recv
   :project: XINIM

.. doxygenfunction:: net::shutdown
   :project: XINIM
