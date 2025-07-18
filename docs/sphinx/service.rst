Service Manager
===============

The service manager maintains critical user-space services and their
dependencies. Services are registered with a dependency list forming a
directed acyclic graph. If a service crashes the manager restarts it
along with all dependents.

Restart Policies
----------------
Each service registration optionally specifies an automatic restart
limit. The limit is stored in a *liveness contract* which also tracks
restart counts. When the limit is reached the manager refuses to
restart the service again.

Additional dependencies may be declared at runtime using
:cpp:func:`svc::ServiceManager::add_dependency`. The restart limit can
be changed dynamically through
:cpp:func:`svc::ServiceManager::set_restart_limit`.

For API-level details see :doc:`service_manager`.

Service Control Utility
-----------------------
The ``svcctl`` command communicates with the service manager using lattice IPC.
It supports the following subcommands::

   svcctl list
   svcctl start <pid>
   svcctl stop <pid>
   svcctl restart <pid>

The ``list`` subcommand prints all registered services along with their running
state. The other subcommands request that the service manager manipulate the
specified service.
