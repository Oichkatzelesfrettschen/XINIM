Scheduler == == == == =

                          The scheduler manages cooperative multitasking inside the
                              kernel.It exposes a FIFO run queue along with a direct yield primitive
                                  .

                                  ..doxygenclass::sched::Scheduler : project : XINIM : members :

    The : cpp :`yield_to` method enables fast hand -
                          off between communicating threads.
