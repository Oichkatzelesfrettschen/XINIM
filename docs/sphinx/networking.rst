Networking Driver
=================

The UDP networking layer transports lattice IPC packets between nodes.  A node
binds to a local UDP port and registers remote peers through
:cpp:func:`net::add_remote`.  The driver spawns a background thread to poll the
socket and invokes an optional callback whenever a packet arrives.

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

Local Node Identification
------------------------

``local_node()`` reads back the IPv4 address assigned to the socket opened by
``net::init``. The driver invokes ``getsockname`` and converts the address into
host byte order. The resulting integer uniquely identifies this host when
exchanging packets.

Registering Remote Peers
-----------------------

Use ``net::add_remote`` to associate a numeric node identifier with a
``host:port`` pair. Datagrams are only transmitted to nodes present in this
mapping. Typically applications register peers immediately after initializing
the driver so that outbound traffic succeeds.

Typical Configuration Steps
---------------------------

1. Call ``net::init`` with a :cpp:struct:`net::Config` describing the local node
   and UDP port.
2. Add remote nodes with :cpp:func:`net::add_remote`.
3. Optionally install a receive callback using
   :cpp:func:`net::set_recv_callback`.

After these steps packets can be sent with :cpp:func:`net::send` and consumed
via :cpp:func:`net::recv`.

Example Two-Node Exchange
-------------------------

The unit tests spawn two processes communicating over localhost. The following
snippet mirrors that setup while omitting error handling:

.. code-block:: cpp

   constexpr net::node_t PARENT_NODE = 0;      // identifier for parent
   constexpr net::node_t CHILD_NODE  = 1;      // identifier for child
   constexpr uint16_t PARENT_PORT = 14000;     // UDP port for parent
   constexpr uint16_t CHILD_PORT  = 14001;     // UDP port for child

   if (fork() == 0) {
       // Child process configuration
       net::init({CHILD_NODE, CHILD_PORT});
       net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
       std::array<std::byte, 1> ready{std::byte{0}}; // notify parent
       net::send(PARENT_NODE, ready);

       net::Packet pkt{}; // wait for message from parent
       while (!net::recv(pkt)) {
           std::this_thread::sleep_for(10ms);
       }

       std::array<std::byte, 3> reply{
           std::byte{9}, std::byte{8}, std::byte{7}}; // send reply
       net::send(PARENT_NODE, reply);
       std::this_thread::sleep_for(50ms);
       net::shutdown();
       std::exit(0);
   }

   // Parent process setup
   net::init({PARENT_NODE, PARENT_PORT});
   net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);

   net::Packet pkt{};
   while (!net::recv(pkt)) { // wait for child readiness
       std::this_thread::sleep_for(10ms);
   }

   std::array<std::byte, 3> data{
       std::byte{1}, std::byte{2}, std::byte{3}}; // send data
   net::send(CHILD_NODE, data);

   do { // await reply
       std::this_thread::sleep_for(10ms);
   } while (!net::recv(pkt));

   net::shutdown();
