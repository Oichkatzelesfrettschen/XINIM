Scheduler
=========

The scheduler manages cooperative multitasking inside the kernel. It exposes a
FIFO run queue and a direct yield primitive.

Blocking
--------

The implementation integrates deadlock detection. Threads may block on one
another through :cpp:func:`sched::Scheduler::block_on` and
:cpp:func:`sched::Scheduler::unblock`. Blocking relationships are tracked in a
wait-for graph and cycles trigger immediate failure.

.. doxygenclass:: sched::Scheduler
   :project: XINIM
   :members:

Message Hand-off
----------------

Calling :cpp:func:`lattice::lattice_send` delivers a message immediately when the
receiver is already waiting. On such a fast path the scheduler invokes
:cpp:func:`sched::Scheduler::yield_to` so the receiver handles the message
without an extra context switch. Both send and
:cpp:func:`lattice::lattice_recv` accept :cpp:type:`lattice::IpcFlags` with the
``NONBLOCK`` option to return immediately when no delivery is possible. Without
that flag the scheduler blocks the caller until a message arrives or a 100 ms
timeout elapses.

.. doxygenfunction:: sched::Scheduler::yield_to
   :project: XINIM
