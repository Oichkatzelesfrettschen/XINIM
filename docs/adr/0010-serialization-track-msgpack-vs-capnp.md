# ADR 0010: Serialization Track - MessagePack vs Cap'n Proto

Date: 2026-03-08
Status: Proposed

## Context

The repository does not currently ship a runtime serialization stack based on
MessagePack or Cap'n Proto. This decision record exists so future protocol work
does not turn into an untracked dependency detour.

The immediate build and boot priorities remain:
- pure Conan + CMake
- repo-local boot tooling
- 32-bit x86 bring-up
- `xash` and native tool growth

## Decision

Do not add MessagePack or Cap'n Proto to the runtime or build graph in this
slice.

Instead:
- keep this as a tracked architecture question
- evaluate it only when a concrete wire-format surface exists
- require evidence on latency, footprint, and implementation cost before
  adoption

## Evaluation Criteria

- total message cost, not just serializer microbenchmarks
- CPU time on low-end x86 targets, including 486-safe paths where relevant
- code size and memory footprint
- allocation behavior
- schema evolution story
- zero-copy practicality versus complexity
- hosted tooling ergonomics
- freestanding and low-level integration cost

## Current Evidence Notes

- Current repo reality: no live MessagePack or Cap'n Proto dependency exists.
- Cap'n Proto's official language documentation mentions MessagePack as a
  contrast in typing and self-description, not as a performance endorsement:
  https://capnproto.org/language.html

## Consequences

- The project avoids premature dependency churn.
- Future serialization work has a named place to land.
- Any eventual adoption should come with lane-aware benchmarks and a clear
  protocol owner.
