# i486 Service Supervision Synthesis

Date: 2026-03-10

This note records the current synthesis direction for the i486 lane: keep the
kernel-side supervision path tiny and freestanding, but align its vocabulary
and control flow with the repository's broader reincarnation-server and
recovery-DAG work.

## Why This Exists

Before this step, the live i486 lane treated the native `mksh` init process as
a one-off special case inside `src/kernel/i486/ring3.cpp`:

- one global `g_shell_process`
- one shell-only reload path
- one shell-only fault distinction

That worked for bring-up, but it did not scale into the repo's existing
service and recovery architecture.

## External Design Donor

The primary donor is MINIX 3's RS service manager. The most relevant upstream
reference points used for this synthesis were:

- `minix/servers/rs/main.c`
- `minix/servers/rs/manager.c`

Official upstream source:

- https://github.com/Stichting-MINIX-Research-Foundation/minix
- https://raw.githubusercontent.com/Stichting-MINIX-Research-Foundation/minix/master/minix/servers/rs/main.c
- https://raw.githubusercontent.com/Stichting-MINIX-Research-Foundation/minix/master/minix/servers/rs/manager.c

The imported invariants are deliberately narrow:

1. supervised services have explicit identity
2. restart policy is explicit, not implicit
3. service state transitions are tracked centrally
4. restart is a service-manager action, not a shell-specific one-off
5. restart counts are bounded

## XINIM-Specific Reconciliation

The repo already had three related but disconnected stories:

1. the live i486 boot path in `src/kernel/i486/ring3.cpp`
2. a richer hosted `ReincarnationServer` model in
   `include/xinim/servers/reincarnation_server.hpp`
3. a freestanding recovery DAG in `src/kernel/recovery/`

The chosen synthesis is:

- do not import the hosted STL-heavy RS implementation into the i486 kernel
- do reuse the freestanding recovery vocabulary:
  - `RestartPolicy`
  - `ServiceState`
- do introduce a fixed-capacity supervised-service table in the i486 lane
- do make the native `mksh` init path the first supervised service

This keeps the 486 lane honest about resource constraints while moving it
toward the same conceptual model as the broader repo.

## Current i486 Contract

The i486 lane now treats the native `mksh` init path as a supervised service
with:

- a stable service name
- a file/path/env payload definition
- a tracked PID
- an explicit restart policy
- a bounded restart count
- a tracked service state
- a DAG index in the freestanding recovery graph
- scheduler-policy metadata aligned with the kernel's shared scheduler policy

The supervision metadata is now synchronized with `src/kernel/recovery/`:

- service registration allocates a real `RecoveryDag` node
- successful launch updates DAG PID and state to `RUNNING`
- clean exit and fault handling now consume restart budget through the DAG path
  instead of only local counters
- a second long-lived user payload, `hold-service`, is now registered in the
  same supervision table as an on-demand support service and declared as
  dependent on the init service

The current policy for the init service is:

- restart policy: `RESTART`
- clean-exit respawn: enabled
- bounded restart count: enabled
- rescue fallback: only after supervised relaunch failure or restart-limit hit

## What This Changes

The practical effect is that the i486 lane no longer says:

- "the shell respawns because the kernel knows about one special shell"

It now says:

- "the supervised init service restarts because the kernel tracks a service
  record with explicit restart policy"

That is the right bridge between today's tiny kernel lane and tomorrow's
multi-service supervision story.

## Next Synthesis Steps

1. keep the init service as service slot 0 and prove the restart-limit path
2. add the timer-driven scheduler work needed before multiple long-lived user
   services can run concurrently on the i486 lane
3. teach the i486 service runtime to walk multi-service DAG restart order once
   the lane can actually schedule more than one service at a time
4. only after that, consider lifting more of the broader RS model into the
   small-lane kernel or moving supervision into userspace
