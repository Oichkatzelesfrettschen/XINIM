Networking Driver
=================

The UDP networking layer transports lattice IPC packets between nodes.  A node
binds to a local UDP port and registers remote peers through
:cpp:func:`net::add_remote`.  The driver spawns background threads to poll the
UDP socket and accept optional TCP connections. Packets are delivered over UDP
unless the peer was added with ``tcp=true``.

The host identifier is computed by hashing ``gethostname`` with
:cpp:func:`net::local_node`, providing a unique node ID across the cluster.

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

.. doxygenfunction:: net::local_node
   :project: XINIM

.. doxygenfunction:: net::set_recv_callback
   :project: XINIM

.. doxygenfunction:: net::send
   :project: XINIM

.. doxygenfunction:: net::recv
   :project: XINIM

.. doxygenfunction:: net::shutdown
   :project: XINIM
