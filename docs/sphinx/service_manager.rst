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

Querying Services
-----------------

The manager exposes helpers for inspection. Call
:cpp:func:`svc::ServiceManager::list_services` to enumerate registered
services and :cpp:func:`svc::ServiceManager::dependencies` to inspect a
service's direct dependencies.

.. code-block:: cpp

   svc::service_manager.register_service(1);
   svc::service_manager.register_service(2, {1});
   auto ids = svc::service_manager.list_services();      // {1, 2}
   auto deps = svc::service_manager.dependencies(2);     // {1}

.. doxygenclass:: svc::ServiceManager
   :project: XINIM
   :members:
