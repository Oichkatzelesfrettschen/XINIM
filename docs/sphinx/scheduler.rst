Scheduler
=========

The scheduler manages cooperative multitasking inside the kernel. It exposes a
FIFO run queue along with a direct yield primitive for fast context switches.

Blocking
--------

The implementation integrates deadlock detection. Threads may block on one
another through :cpp:`sched::Scheduler::block_on` and
:cpp:`sched::Scheduler::unblock`. Blocking relationships are tracked in a
wait-for graph and cycles trigger immediate failure.

.. doxygenclass:: sched::Scheduler
   :project: XINIM
   :members:

Message Hand-off
----------------

Calling :cpp:`lattice::lattice_send` delivers a message immediately when the
receiver is already waiting. In that case execution yields directly to the
receiver via :cpp:`sched::Scheduler::yield_to` so the message handler runs
without delay.

.. doxygenfunction:: sched::Scheduler::yield_to
   :project: XINIM
