Networking Driver
=================

The networking driver transports Lattice IPC packets between nodes over UDP or, optionally, TCP.  Each node:

- **Binds** to a local UDP port (and a TCP listen socket if TCP‐enabled).  
- **Registers** peers via :cpp:func:`net::add_remote(node, host, port, tcp?)`.  
- **Spawns** background threads to receive UDP datagrams and accept TCP connections.  
- **Queues** incoming packets internally and invokes an optional callback.  
- **Delivers** packets via :cpp:func:`net::recv`, or via the registered callback.  
- **Shuts down** cleanly with :cpp:func:`net::shutdown`.

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
Every node assigns itself a numeric ``node_t`` identifier during
:cpp:func:`net::init`.  The configuration structure passed to
``net::init`` specifies both the local node ID and the UDP port to bind.
Once initialized, :cpp:func:`net::local_node()` returns this identifier
and it becomes the source ID for all outgoing packets.  Peers use this
value to authenticate who sent each message.


Local Node Identification
-------------------------
:cpp:func:`net::local_node` opens its UDP socket (via :cpp:func:`net::init`),  
calls ``getsockname()``, and returns the bound IPv4 address in host byte order  
as a stable ``node_t`` identifier.

Registering Remote Peers
------------------------
Use:

.. code-block:: cpp

   net::add_remote(node_id, "hostname-or-ip", port, /*tcp=*/false);

to associate a numeric ``node_id`` with a host:port.  Only packets to registered  
peers are transmitted.  For TCP, pass ``tcp=true``.

Typical Configuration Steps
---------------------------
1. **Initialize** the driver:

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
   net::send(2, std::array<std::byte,3>{'h','i','!'});  // greet B

   // node B initialization
   net::init({2, 12001});  // bind port and assign ID 2
   net::add_remote(1, "127.0.0.1", 12000, /*tcp=*/false);  // register node A
   net::Packet pkt{};  // buffer for incoming packet
   while (!net::recv(pkt)) { /* wait for greeting */ }
   net::send(1, std::array<std::byte,3>{'o','k','!'});  // reply to A

Example: Two‐Node Exchange
--------------------------
This example shows a parent and child process exchanging small payloads over UDP.

.. code-block:: cpp

   #include <array>
   #include <thread>
   #include <chrono>
   #include <cassert>
   #include <unistd.h>
   #include <sys/wait.h>
   #include "net_driver.hpp"

   using namespace std::chrono_literals;
   constexpr net::node_t   PARENT_NODE = 0, CHILD_NODE = 1;
   constexpr uint16_t      PARENT_PORT = 14000, CHILD_PORT = 14001;

   int parent_proc(pid_t child) {
       net::init({PARENT_NODE, PARENT_PORT});
       net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT, /*tcp=*/false);

       // wait for readiness signal
       net::Packet pkt;
       while (!net::recv(pkt)) std::this_thread::sleep_for(10ms);
       assert(pkt.src_node == CHILD_NODE);

       // send data
       std::array<std::byte,3> data{1,2,3};
       net::send(CHILD_NODE, data);

       // await reply
       while (!net::recv(pkt)) std::this_thread::sleep_for(10ms);
       assert(pkt.src_node == CHILD_NODE);
       assert(pkt.payload == std::vector<std::byte>{9,8,7});

       waitpid(child, nullptr, 0);
       net::shutdown();
       return 0;
   }

   int child_proc() {
       net::init({CHILD_NODE, CHILD_PORT});
       net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT, /*tcp=*/false);

       // signal readiness
       net::send(PARENT_NODE, std::array<std::byte,1>{0});

       // receive payload
       net::Packet pkt;
       while (!net::recv(pkt)) std::this_thread::sleep_for(10ms);
       assert(pkt.src_node == PARENT_NODE);

       // reply
       net::send(PARENT_NODE, std::array<std::byte,3>{9,8,7});
       net::shutdown();
       return 0;
   }

   int main() {
       pid_t pid = fork();
       if (pid == 0) {
           return child_proc();
       } else {
           return parent_proc(pid);
       }
   }
