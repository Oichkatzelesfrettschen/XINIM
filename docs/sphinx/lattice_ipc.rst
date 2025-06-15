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

.. doxygenfunction:: fastpath::execute_fastpath
   :project: XINIM

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
