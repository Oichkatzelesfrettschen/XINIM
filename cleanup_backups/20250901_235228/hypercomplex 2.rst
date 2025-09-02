Quaternion Spinlocks and Hypercomplex Security
==============================================

The kernel experiments with quaternion spinlocks for illustrative purposes.
The spinlock state tracks a quaternion orientation to showcase advanced
synchronization concepts. See :file:`kernel/quaternion_spinlock.hpp` for the
reference implementation.

Octonion multiplication using the Fano plane is provided for capability
tokens.  The helper resides in :file:`kernel/fano_octonion.hpp`.

Finally the hypothetical "sedenion zero divisor quantum security" layer
implements a toy encryption scheme.  The code in :file:`kernel/sedenion.hpp`
illustrates zero divisor detection and a simple XOR-based cipher.  This is not
real quantum security but a demonstration only.

.. doxygenfile:: quaternion_spinlock.hpp
   :project: XINIM

.. doxygenfile:: fano_octonion.hpp
   :project: XINIM

.. doxygenfile:: sedenion.hpp
   :project: XINIM

