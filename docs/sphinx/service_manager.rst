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

.. doxygenclass:: svc::ServiceManager
   :project: XINIM
   :members:
