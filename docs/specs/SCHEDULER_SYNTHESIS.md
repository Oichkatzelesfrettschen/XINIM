# Scheduler Synthesis

Date: 2026-03-10

This document records the current scheduler inventory in the repository and
the chosen convergence direction.

## Repo Inventory

The repository currently contains four scheduler stories:

1. `src/kernel/proc.cpp`
   - MINIX-heritage priority queues and `pick_proc()`
   - tightly coupled to the legacy process table and IPC path

2. `src/kernel/schedule.hpp` and `src/kernel/schedule.cpp`
   - small fixed-size FIFO / wait-graph scheduler used heavily by host tests
   - useful as a compact model, but not the canonical kernel scheduler

3. `src/kernel/scheduler.hpp`, `src/kernel/scheduler.cpp`,
   `src/kernel/unified_scheduler.hpp`, and `src/kernel/unified_scheduler.cpp`
   - the intended authoritative kernel scheduler
   - PCB-based, O(1) bitmap priority queues, wait-graph integration,
     preemptive timer-driven policy

4. `src/kernel/i486/ring3.cpp`
   - an i486-specific cooperative process handoff path
   - real Ring 3 bring-up, but not yet a timer-driven scheduler

## External Donors

### MINIX 3

Useful scheduler ideas imported from MINIX:

- strict priority queues
- runtime penalty on quantum exhaustion
- periodic restoration toward baseline priority
- service-oriented scheduling context

Primary sources reviewed:

- MINIX kernel/process heritage in `proc.c`:
  https://raw.githubusercontent.com/Stichting-MINIX-Research-Foundation/minix/master/minix/kernel/proc.c
- MINIX scheduling documentation and user-mode scheduling discussion:
  https://wiki.minix3.org/doku.php?id=developersguide:userspacescheduling
  https://wiki.minix3.org/lib/exe/fetch.php?media=developersguide:userspaceschedulingreport.pdf

### L4

Useful scheduler ideas imported from L4:

- priority and quantum are separate knobs
- scheduling parameters should be explicit, not hidden in hard-coded tables
- scheduling domains / partitions matter, even if the first implementation is
  single-CPU and single-domain

Primary source reviewed:

- official L4Re scheduler API:
  https://l4re.org/doc/group__l4__scheduler__api.html

## Chosen Synthesis

The canonical scheduler direction for XINIM is:

- PCB-based
- fixed-capacity and freestanding
- priority-queue based
- explicit quantum aware
- deadlock-aware for blocking relationships
- able to support service supervision later

That means the canonical policy lives in `UnifiedScheduler`, not in the older
FIFO `sched::Scheduler` model and not in the i486 ad hoc handoff path.

## Implemented In This Slice

The first harmonization step landed in `UnifiedScheduler`:

- effective priority is now distinct from baseline priority
- quantum may be explicitly configured per process
- quantum exhaustion demotes runnable user work by one priority step
- periodic rebalancing restores processes toward baseline priority
- shared scheduler-policy constants and quantum rules now live in
  `src/kernel/scheduler_policy.hpp` so the active kernel scheduler and the i486
  supervised-service table consume the same priority vocabulary
- the live IRQ timer stub now feeds `g_unified_scheduler.timer_tick()` before
  entering the scheduler facade, so the active timer hook no longer drops all
  scheduler accounting on the floor
- `pipe.cpp` now blocks and wakes processes through `scheduler.hpp` instead of
  mutating PCB state behind the scheduler's back

This is the intended blend:

- MINIX-style runtime penalty and boost
- L4-style explicit quantum knob
- existing XINIM wait-graph and O(1) bitmap queues

## What Remains

The remaining unification work is explicit:

1. `scheduler.cpp` still straddles unified policy and legacy `pick_proc()`
   for final runnable selection on the legacy bare-metal path
2. the host-only `sched::Scheduler` contract must either become a thin adapter
   over the canonical scheduler semantics or be clearly scoped as a test model
3. the i486 Ring 3 lane must adopt the same scheduler policy in place of the
   current direct process handoff path
4. the i486 lane then needs real IRQ/timer-driven preemption before it can
   claim full multi-service scheduling

## i486 Direction

For the i486 lane, the honest next staircase is:

1. keep the current supervised-service and Ring 3 boot path stable
2. replace the ad hoc i486 ready/wait handoff logic with a tiny scheduler core
   that matches `UnifiedScheduler` policy
3. add PIT / PIC / IRQ delivery for the i486 lane and wire it into that core
4. then enable true time-sliced service concurrency

That keeps the 486 lane honest while still moving it toward the repository's
single scheduler story.
