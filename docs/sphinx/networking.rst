Networking Driver
=================

The networking driver transports Lattice IPC packets between nodes over UDP or, optionally, TCP.  Each node:

- **Binds** to a local UDP port (and a TCP listen socket if TCP‐enabled).  
- **Registers** peers via :cpp:func:`net::add_remote`.
- **Spawns** background threads to receive UDP datagrams and accept TCP connections.  
- **Queues** incoming packets internally and invokes an optional callback.  
- **Delivers** packets via :cpp:func:`net::recv`, or via the registered callback.  
- **Shuts down** cleanly with :cpp:func:`net::shutdown`.

When ``Config::node_id`` is set to ``0`` the driver derives a unique identifier
by hashing the primary network interface.  This automatically detected value is
returned by :cpp:func:`net::local_node` unless a different identifier is
provided in the configuration.

API Overview
------------
.. doxygentypedef:: net::Config
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

Node Identity
-------------
Each node assigns itself a numeric ``node_t`` identifier when
:cpp:func:`net::init` executes.  The ``node_id`` and UDP port are
provided via the configuration structure along with ``node_id_dir``
which specifies where the identifier is persisted.  After initialization,
:cpp:func:`net::local_node` reports this identifier and all outgoing
packets carry it as the source ID so peers can validate who originated
each message.


Local Node Identification
-------------------------
:cpp:func:`net::local_node` first verifies that ``net::init`` provided a
non-zero ``node_id``. If it did, the identifier is returned unchanged.
Otherwise the driver enumerates network interfaces using platform-specific
APIs—``getifaddrs`` on Linux and the BSD family or ``GetAdaptersAddresses`` on
Windows—and hashes the first active device that is not a loopback interface.
Should this process fail, the driver falls back to hashing the local host name.
The computed identifier is non-zero and remains constant for the lifetime of
the process. When the identifier is computed it is written to
``node_id_dir/node_id`` so that subsequent invocations of :cpp:func:`net::init`
reuse the same value. If running without root privileges and
``node_id_dir`` is unspecified, the driver defaults to
``$XDG_STATE_HOME/xinim`` or ``$HOME/.xinim``.

Implementation Steps
~~~~~~~~~~~~~~~~~~~~
The internal logic of :cpp:func:`net::local_node` unfolds in these steps:

#. If ``Config::node_id`` is non-zero return it immediately.
#. Invoke the platform specific API to enumerate interfaces.
#. Iterate until the first device flagged ``IFF_UP`` and not ``IFF_LOOPBACK`` is
   found.
   * If a link-layer (MAC) address is present, hash its bytes.
   * Otherwise hash the IPv4 or IPv6 address.
#. Release the interface list.
#. If a valid interface produced a hash, return it.
#. As a fallback obtain the hostname via ``gethostname`` and hash that value.

.. note::
   IPv6 addresses are now supported for remote peers. When deriving the
   local identifier the driver hashes the first non-loopback MAC, IPv4 or
   IPv6 address. The identifier may still change when network hardware
   changes.

Registering Remote Peers
------------------------
A node communicates only with peers explicitly added using
:cpp:func:`net::add_remote`::

   net::add_remote(node_id, "hostname-or-ip", port, /*tcp=*/false);

The ``node_id`` uniquely identifies the peer.  The ``host`` and ``port``
parameters supply its address. ``host`` accepts IPv4 or IPv6 literals or a
hostname. Set ``tcp=true`` to create a persistent TCP
connection; otherwise UDP datagrams are used.  Packets are sent only to
registered peers and looked up by ``node_id`` at transmission time.



Typical Configuration Steps
---------------------------
1. **Initialize** the driver.  Pass ``0`` as ``node_id`` to let
   :cpp:func:`net::local_node` derive the identifier from an active network
   interface:

   .. code-block:: cpp

      net::init({ local_node_id, udp_port });

2. **Register** remote peers:

   .. code-block:: cpp

      net::add_remote(remote_node, "192.168.1.5", 15000, /*tcp=*/false);

3. **(Optional)** Install a receive callback:

   .. code-block:: cpp

      net::set_recv_callback([](const net::Packet &pkt){
          // handle incoming packet
      });

4. **Send** and **receive**:

   .. code-block:: cpp

      net::send(dest_node, payload_bytes);
      net::Packet pkt;
      if (net::recv(pkt)) {
          // process pkt.payload
      }

5. **Shutdown** when done:

   .. code-block:: cpp

      net::shutdown();

Simple Registration Example
---------------------------
This brief code sample demonstrates two nodes registering each other and
exchanging a greeting over UDP.

.. code-block:: cpp

   // node A initialization
   net::init({1, 12000});  // bind port and assign ID 1
   net::add_remote(2, "127.0.0.1", 12001, /*tcp=*/false);  // register node B
   net::add_remote(3, "::1", 12002, /*tcp=*/false);        // IPv6 loopback
   net::send(2, std::array<std::byte,3>{'h','i','!'});  // greet B

   // node B initialization
   net::init({2, 12001});  // bind port and assign ID 2
   net::add_remote(1, "127.0.0.1", 12000, /*tcp=*/false);  // register node A
   net::add_remote(3, "::1", 12002, /*tcp=*/false);        // IPv6 loopback
   net::Packet pkt{};  // buffer for incoming packet
   while (!net::recv(pkt)) { /* wait for greeting */ }
   net::send(1, std::array<std::byte,3>{'o','k','!'});  // reply to A

Example: Two‐Node Exchange
--------------------------
This example shows a parent and child process exchanging small payloads over UDP.

Example: Two-Node Handshake
---------------------------
The :file:`tests/test_net_two_node.cpp` unit test spawns a parent and child
process that exchange a handshake. The child echoes its
:cpp:func:`net::local_node` value so the parent can verify unique identifiers.

.. code-block:: cpp

   #include <chrono>           // std::chrono literals
   #include <thread>           // sleep while polling
   #include <cassert>
   #include <unistd.h>         // fork and waitpid
   #include "lattice_ipc.hpp"
   #include "net_driver.hpp"

   using namespace lattice;
   using namespace std::chrono_literals;

   constexpr net::node_t PARENT_NODE = 0;   ///< ID for the parent
   constexpr net::node_t CHILD_NODE  = 1;   ///< ID for the child
   constexpr std::uint16_t PARENT_PORT = 13000; ///< Parent UDP port
   constexpr std::uint16_t CHILD_PORT  = 13001; ///< Child UDP port

   // Child waits for a handshake then replies with its node ID
   int child_proc() {
       net::init({CHILD_NODE, CHILD_PORT});
       net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
       g_graph = Graph{};
       lattice_connect(2, 1, PARENT_NODE);

       message incoming{};
       while (true) {                // poll until handshake arrives
           poll_network();
           if (lattice_recv(1, &incoming) == OK) break;
           std::this_thread::sleep_for(10ms);
       }

       message reply{};
       reply.m_type = net::local_node();
       lattice_send(2, 1, reply);
       net::shutdown();
       return 0;
   }

   // Parent sends the handshake and verifies the response
   int parent_proc(pid_t child) {
       net::init({PARENT_NODE, PARENT_PORT});
       net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);
       g_graph = Graph{};
       lattice_connect(1, 2, CHILD_NODE);

       message hi{};
       hi.m_type = 0x1234;
       lattice_send(1, 2, hi);

       message reply{};
       while (true) {                // poll until reply arrives
           poll_network();
           if (lattice_recv(2, &reply) == OK) break;
           std::this_thread::sleep_for(10ms);
       }

       assert(reply.m_type != net::local_node());
       waitpid(child, nullptr, 0);
       net::shutdown();
       return 0;
   }

   int main() {
       pid_t pid = fork();
       if (pid == 0) {
           return child_proc();
       }
       return parent_proc(pid);
   }

Networking Driver
=================

The networking driver transports Lattice IPC packets between nodes over UDP or, optionally, TCP.  Each node:

- **Binds** to a local UDP port (and a TCP listen socket if TCP‐enabled).  
- **Registers** peers via :cpp:func:`net::Driver::add_remote`.
- **Spawns** background threads to receive UDP datagrams and accept TCP connections.  
- **Queues** incoming packets internally and invokes an optional callback.  
- **Delivers** packets via :cpp:func:`net::Driver::recv`, or via the registered callback.
- **Shuts down** cleanly with :cpp:func:`net::Driver::shutdown`.

When ``Config::node_id`` is set to ``0`` the driver derives a unique identifier
by hashing the primary network interface.  This automatically detected value is
returned by :cpp:func:`net::Driver::local_node` unless a different identifier is
provided in the configuration.

API Overview
------------
.. doxygentypedef:: net::Config
   :project: XINIM

.. doxygentypedef:: net::RecvCallback
   :project: XINIM

.. doxygenfunction:: net::Driver::init
   :project: XINIM

.. doxygenfunction:: net::Driver::add_remote
   :project: XINIM

.. doxygenfunction:: net::Driver::local_node
   :project: XINIM

.. doxygenfunction:: net::Driver::set_recv_callback
   :project: XINIM

.. doxygenfunction:: net::Driver::send
   :project: XINIM

.. doxygenfunction:: net::Driver::recv
   :project: XINIM

.. doxygenfunction:: net::Driver::shutdown
   :project: XINIM

Node Identity
-------------
Each node assigns itself a numeric ``node_t`` identifier when
:cpp:func:`net::Driver::init` executes.  The ``node_id`` and UDP port are
provided via the configuration structure.  After initialization,
:cpp:func:`net::local_node` reports this identifier and all outgoing
packets carry it as the source ID so peers can validate who originated
each message.


Local Node Identification
-------------------------
:cpp:func:`net::Driver::local_node` first verifies that ``net::Driver::init`` provided a
non-zero ``node_id``. If it did, the identifier is returned unchanged.
Otherwise the driver enumerates network interfaces using platform-specific
APIs—``getifaddrs`` on Linux and the BSD family or ``GetAdaptersAddresses`` on
Windows—and hashes the first active device that is not a loopback interface.
Should this process fail, the driver falls back to hashing the local host name.
The computed identifier is non-zero and remains constant for the lifetime of
the process. When the identifier is computed it is written to
``/etc/xinim/node_id`` so that subsequent invocations of :cpp:func:`net::Driver::init`
reuse the same value.

Implementation Steps
~~~~~~~~~~~~~~~~~~~~
The internal logic of :cpp:func:`net::local_node` unfolds in these steps:

#. If ``Config::node_id`` is non-zero return it immediately.
#. Invoke the platform specific API to enumerate interfaces.
#. Iterate until the first device flagged ``IFF_UP`` and not ``IFF_LOOPBACK`` is
   found.
   * If a link-layer (MAC) address is present, hash its bytes.
   * Otherwise hash the IPv4 or IPv6 address.
#. Release the interface list.
#. If a valid interface produced a hash, return it.
#. As a fallback obtain the hostname via ``gethostname`` and hash that value.

.. note::
   IPv6 addresses are now supported for remote peers. When deriving the
   local identifier the driver hashes the first non-loopback MAC, IPv4 or
   IPv6 address. The identifier may still change when network hardware
   changes.

Registering Remote Peers
------------------------
A node communicates only with peers explicitly added using
:cpp:func:`net::Driver::add_remote`::

   net::driver.add_remote(node_id, "hostname-or-ip", port, /*tcp=*/false);

The ``node_id`` uniquely identifies the peer.  The ``host`` and ``port``
parameters supply its address. ``host`` accepts IPv4 or IPv6 literals or a
hostname. Set ``tcp=true`` to create a persistent TCP
connection; otherwise UDP datagrams are used.  Packets are sent only to
registered peers and looked up by ``node_id`` at transmission time.



Typical Configuration Steps
---------------------------
1. **Initialize** the driver.  Pass ``0`` as ``node_id`` to let
   :cpp:func:`net::local_node` derive the identifier from an active network
   interface:

   .. code-block:: cpp

      net::driver.init({ node_id, udp_port });

2. **Register** remote peers:

   .. code-block:: cpp

      net::driver.add_remote(remote_node, "192.168.1.5", 15000, /*tcp=*/false);

3. **(Optional)** Install a receive callback:

   .. code-block:: cpp

      net::driver.set_recv_callback([](const net::Packet &pkt){
          // handle incoming packet
      });

4. **Send** and **receive**:

   .. code-block:: cpp

      net::driver.send(dest_node, payload_bytes);
      net::Packet pkt;
      if (net::driver.recv(pkt)) {
          // process pkt.payload
      }

5. **Shutdown** when done:

   .. code-block:: cpp

      net::driver.shutdown();

Simple Registration Example
---------------------------
This brief code sample demonstrates two nodes registering each other and
exchanging a greeting over UDP.

.. code-block:: cpp

   // node A initialization
   net::driver.init({1, 12000});  // bind port and assign ID 1
   net::driver.add_remote(2, "127.0.0.1", 12001, /*tcp=*/false);  // register node B
   net::driver.add_remote(3, "::1", 12002, /*tcp=*/false);        // IPv6 loopback
   net::driver.send(2, std::array<std::byte,3>{'h','i','!'});  // greet B

   // node B initialization
   net::driver.init({2, 12001});  // bind port and assign ID 2
   net::driver.add_remote(1, "127.0.0.1", 12000, /*tcp=*/false);  // register node A
   net::driver.add_remote(3, "::1", 12002, /*tcp=*/false);        // IPv6 loopback
   net::Packet pkt{};  // buffer for incoming packet
   while (!net::driver.recv(pkt)) { /* wait for greeting */ }
   net::driver.send(1, std::array<std::byte,3>{'o','k','!'});  // reply to A

Example: Two‐Node Exchange
--------------------------
This example shows a parent and child process exchanging small payloads over UDP.

Example: Two-Node Handshake
---------------------------
The :file:`tests/test_net_two_node.cpp` unit test spawns a parent and child
process that exchange a handshake. The child echoes its
:cpp:func:`net::local_node` value so the parent can verify unique identifiers.

.. code-block:: cpp

   #include <chrono>           // std::chrono literals
   #include <thread>           // sleep while polling
   #include <cassert>
   #include <unistd.h>         // fork and waitpid
   #include "lattice_ipc.hpp"
   #include "net_driver.hpp"

   using namespace lattice;
   using namespace std::chrono_literals;

   constexpr net::node_t PARENT_NODE = 0;   ///< ID for the parent
   constexpr net::node_t CHILD_NODE  = 1;   ///< ID for the child
   constexpr std::uint16_t PARENT_PORT = 13000; ///< Parent UDP port
   constexpr std::uint16_t CHILD_PORT  = 13001; ///< Child UDP port

   // Child waits for a handshake then replies with its node ID
   int child_proc() {
       net::driver.init({CHILD_NODE, CHILD_PORT});
       net::driver.add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
       g_graph = Graph{};
       lattice_connect(2, 1, PARENT_NODE);

       message incoming{};
       while (true) {                // poll until handshake arrives
           poll_network();
           if (lattice_recv(1, &incoming) == OK) break;
           std::this_thread::sleep_for(10ms);
       }

       message reply{};
       reply.m_type = net::driver.local_node();
       lattice_send(2, 1, reply);
       net::driver.shutdown();
       return 0;
   }

   // Parent sends the handshake and verifies the response
   int parent_proc(pid_t child) {
       net::driver.init({PARENT_NODE, PARENT_PORT});
       net::driver.add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);
       g_graph = Graph{};
       lattice_connect(1, 2, CHILD_NODE);

       message hi{};
       hi.m_type = 0x1234;
       lattice_send(1, 2, hi);

       message reply{};
       while (true) {                // poll until reply arrives
           poll_network();
           if (lattice_recv(2, &reply) == OK) break;
           std::this_thread::sleep_for(10ms);
       }

       assert(reply.m_type != net::driver.local_node());
       waitpid(child, nullptr, 0);
       net::driver.shutdown();
       return 0;
   }

   int main() {
       pid_t pid = fork();
       if (pid == 0) {
           return child_proc();
       }
       return parent_proc(pid);
   }

