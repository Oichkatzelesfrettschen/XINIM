Service Manager Contracts
=========================

This document expands on how services are registered with the kernel's service
manager, how dependencies are resolved and how crashed services are
automatically restarted.

Service Registration
--------------------

Services register with :cpp:func:`svc::ServiceManager::register_service` and may
specify a list of dependencies. A dependency implies the service will be
restarted after its parents to ensure correct ordering.

Dependencies
------------

Dependencies form a directed acyclic graph. The manager verifies new edges do not
introduce cycles using an internal search routine.

Automatic Restarts
------------------

Whenever a service terminates unexpectedly the scheduler notifies the service
manager. Contracts track how many times each service has been restarted via the
``RestartContract`` structure. Dependents of the crashed service are restarted in
order to preserve required ordering.

Cycle Prevention and Restart Ordering
------------------------------------

Every dependency forms a directed edge in a DAG. When adding new edges the
service manager calls an internal search routine to ensure no path already leads
back to the prospective child. This guarantees the DAG remains acyclic and
restart order stays deterministic. Services always restart in topological order
starting from the original failure up through all dependents.

Example Usage
-------------

The snippet below demonstrates registering three services with dependencies and
how a crash of the root service triggers automatic restarts of the entire tree.

.. code-block:: cpp

   // Pseudo identifiers for illustrative purposes
   xinim::pid_t a = 1;
   xinim::pid_t b = 2;
   xinim::pid_t c = 3;

   // B depends on A, C depends on both A and B
   svc::service_manager.register_service(a);
   svc::service_manager.register_service(b, {a});
   svc::service_manager.register_service(c, {a, b});

   // A crashes unexpectedly; the manager restarts A, then B and C in order
   svc::service_manager.handle_crash(a);

.. doxygenclass:: svc::ServiceManager
   :project: XINIM
   :members:
