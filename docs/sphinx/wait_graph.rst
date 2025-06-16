Wait-for Graph
==============

The kernel tracks which processes block on others through a directed wait-for graph. Each edge ``A -> B`` means that process ``A`` is waiting on ``B`` to release a resource. When a new edge is inserted the graph performs a reachability check from the destination back to the source. If a path already exists the edge would create a cycle and thus a potential deadlock. In that case the insertion is reverted and the caller is notified, allowing the scheduler to react before the system stalls.

.. doxygenclass:: lattice::WaitForGraph
   :project: XINIM
   :members:

.. doxygenfunction:: lattice::WaitForGraph::add_edge
   :project: XINIM

.. doxygenfunction:: lattice::WaitForGraph::remove_edge
   :project: XINIM

.. doxygenfunction:: lattice::WaitForGraph::clear
   :project: XINIM

