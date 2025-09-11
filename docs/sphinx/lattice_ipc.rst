Lattice IPC Subsystem
=====================

The **Lattice IPC** subsystem provides capability‑based, authenticated and
encrypted message passing in XINIM.  A low‑latency *fastpath* handles local
delivery while a distributed transport moves packets between nodes.

Overview
--------

* **Local Fastpath** – zero‑copy per‑CPU queues with spillover statistics
* **Tiered Caching** – L1 (core), L2 (socket), L3 (node) and a spill region
* **Distributed Messaging** – framed, AEAD‑encrypted packets over UDP/TCP
* **Directed Graph API** – channels tracked in a cycle‑free dependency DAG

Distributed Operation
---------------------

Each ``message`` serialises as::

   [ src_pid | dst_pid | payload_bytes... ]

Pipeline:

1. **Frame** – prefix with ``src_pid`` and ``dst_pid``
2. **Encrypt** – XChaCha20‑Poly1305 using a per‑channel key and nonce
3. **Transmit** – :cpp:func:`net::send`
4. **Receive** – :cpp:func:`net::recv`
5. **Reintegrate** – :cpp:func:`lattice::poll_network`

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
~~~~~~~~~~~~

Channels derive a shared key using post‑quantum (e.g., Kyber) key exchange.
The key feeds XChaCha20‑Poly1305 for authenticated encryption.

Security & Integrity
--------------------

* **Confidentiality** – AEAD with a PQ‑derived key and per‑packet nonce
* **Authentication** – sequence counters and per‑message tokens
* **Thread‑safety** – quaternion spinlocks guard channel state
* **Deadlock Avoidance** – the DAG prevents cycles

See Also
--------

* :file:`kernel/lattice_ipc.hpp`
* :file:`kernel/wormhole.hpp`
* :file:`kernel/net_driver.hpp`
* :file:`kernel/schedule.hpp`

