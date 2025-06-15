Service Manager
===============

The service manager maintains critical user-space services and their
dependencies.  Services are registered with a dependency list forming a
directed acyclic graph.  If a service crashes the manager restarts it
along with all dependents.

.. doxygenclass:: svc::ServiceManager
   :project: XINIM
   :members:
