Process Control
===============

XINIM manages the process table using modern C++ RAII techniques. The
``ScopedProcessSlot`` class acquires a free slot and automatically releases it
on scope exit, ensuring consistent accounting of active processes.

.. doxygenclass:: xinim::ScopedProcessSlot
   :members:
   :undoc-members:
   :project: XINIM
