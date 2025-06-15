Scheduler
=========

The scheduler manages cooperative multitasking inside the
kernel.It exposes a FIFO run queue along with a direct yield primitive
                                  .

The implementation also integrates deadlock detection. Threads may block on
one another through :cpp:`block_on` and :cpp:`unblock`. Blocking relationships
are tracked in a wait-for graph and cycles trigger immediate failure.

                                  ..doxygenclass::sched::Scheduler : project : XINIM : members :


The ``yield_to`` method enables fast hand-off between communicating threads.
