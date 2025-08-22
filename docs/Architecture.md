# System Architecture (XINIM)

XINIM revisits the original MINIX-1 style microkernel through a modern C++23 lens.  
The system is organised into a small number of well-defined layers that separate abstract
mathematics from concrete implementation. Each layer exposes a narrowly scoped interface
and is documented with Doxygen; Sphinx (via the Breathe extension) weaves these APIs and
the architectural narrative into the developer manual.

## Layered Design

- **L0 — Governing Mathematical Model**  
  Formal definitions of security labels and a capability algebra (working model grounded
  in octonions). The intent is to provide a compositional, non-associative algebra that
  makes explicit where capability composition is order-sensitive. This model is a
  *theoretical contract* that higher layers must not violate.

- **L1 — Abstract Kernel Contract**  
  State machines for capability lifecycles, message channels, and scheduling invariants.
  This is the behaviourally complete, implementation-agnostic specification (pre/post
  conditions and safety properties) to which all concrete code is verified or tested.

- **L2 — Algorithmic Realisation**  
  Concrete data structures that satisfy L1, e.g. `lattice::Channel` and
  `sched::Scheduler`. Scheduling is driven by a dependency DAG to enforce causality and
  capability constraints while remaining work-conserving under load.

- **L3 — C++23 Skeleton**  
  Namespaces, classes, and concepts live in headers with Doxygen comments consumed by
  Breathe. Concepts and `static_assert` constraints encode slices of the L1 contract at
  compile time; unit/property tests check the remainder at run time.

- **L4 — Tool Chain**  
  Meson and CMake are maintained in parity for builds with Clang 18; Doxygen generates
  API references consumed by Sphinx to produce the developer manual.

For a comprehensive and continually updated discussion, consult the Sphinx manual’s
[architecture chapter](sphinx/architecture.rst).

---

## Documentation: generating the site

```bash
# From repository root
cd docs

# 1) Generate API docs from C++ headers/sources
doxygen Doxyfile

# 2) Build the Sphinx site (Breathe reads Doxygen XML)
sphinx-build -b html sphinx sphinx/html

# Open the result:
#   docs/sphinx/html/index.html
```

Minimal expectations for the docs toolchain
- Doxygen: XML output enabled and INPUT points at include/ and src/.
- Breathe: Sphinx extension enabled; breathe_projects maps to the Doxygen XML dir.
- Sphinx: sphinx/architecture.rst included in the toctree; uses the Breathe domains.

---

Example: contract-first C++23 skeleton (extract)

```cpp
/// \file include/xinim/capability.hpp
/// \brief Capability primitives (L3) reflecting L1 state machines.

#pragma once
#include <concepts>
#include <cstdint>
#include <string_view>
#include <utility>

namespace xinim::cap {

// L0 label: opaque, but totally ordered for lattice operations.
struct Label {
  std::uint64_t v; // placeholder; L0 algebra defines ordering & composition
  friend constexpr bool operator<(Label a, Label b) noexcept { return a.v < b.v; }
};

// L1 lifecycle states
enum class State : std::uint8_t { Unborn, Live, Revoked };

// C++23 concept capturing the minimal capability surface for L3 types.
template<class T>
concept Capability =
  requires(T t, Label l) {
    // Observers
    { t.label() } -> std::same_as<Label>;
    { t.state() } -> std::same_as<State>;
    // Transitions (must satisfy L1 pre/post; enforced in tests)
    { t.relabel(l) } -> std::same_as<void>;
    { t.revoke() }   -> std::same_as<void>;
  };

/// Example concrete capability (L2 realisation will refine details).
class BasicCap {
  Label lbl_{0};
  State st_{State::Unborn};
public:
  [[nodiscard]] constexpr Label label()  const noexcept { return lbl_; }
  [[nodiscard]] constexpr State state()  const noexcept { return st_;  }
  constexpr void relabel(Label l) noexcept { lbl_ = l; }
  constexpr void revive(Label l)  noexcept { lbl_ = l; st_ = State::Live; }
  constexpr void revoke()         noexcept { st_  = State::Revoked; }
};
static_assert(Capability<BasicCap>);

} // namespace xinim::cap
```

The full L0 algebra (including non-associative composition rules and partial orders)
belongs in docs/specs/l0-capability-algebra.md and is referenced by L1’s state
machines. L2 test vectors should include adversarial orderings to validate the
non-associativity assumptions.

---

Build systems: policy
- Primary build: Meson (meson setup build && meson compile -C build).
- Secondary build: CMake for IDE/tooling parity (keep compile options in sync).
- Compiler: Clang 18 is the reference; CI also exercises GCC to catch portability drift.

---

Quality gates
- L0/L1 are versioned specs; changes require a proofs-and-tests update.
- L2 must pass property tests derived from L1 invariants.
- L3 static_assert/concept checks enforce API misuse at compile time.
- L4 CI publishes Sphinx HTML on tagged releases.

---

## Minimal glue you’ll likely want to add (drop-ins)

**Doxygen** (excerpt; ensure XML is on and Sphinx can find it):

```ini
# Doxyfile (snippets)
PROJECT_NAME           = "XINIM"
GENERATE_XML           = YES
XML_OUTPUT             = xml
INPUT                  = ../include ../src
RECURSIVE              = YES
EXTRACT_ALL            = YES
QUIET                  = YES
```

Sphinx conf.py (snippets):

```python
extensions = ["breathe", "sphinx.ext.autosectionlabel", "sphinx.ext.graphviz"]
breathe_projects = {"xinim": "../doxygen/xml"}
breathe_default_project = "xinim"
```

Sphinx index.rst (ensure the chapter is discoverable):

```rst
.. toctree::
   :maxdepth: 2

   architecture
   api/index
```

---

Commit message suggestion

docs: resolve architecture merge; unify L0–L4, add build/docs instructions

- Merged duplicate architecture overviews and normalised en-GB style.
- Clarified L0 octonion-based algebra as working model; L1 state machines & invariants.
- Kept concrete examples (Channel/Scheduler DAG) without overspecifying internals.
- Added reproducible Doxygen→Breathe→Sphinx pipeline and example C++23 skeleton.

---

Next actions
1. Add docs/specs/l0-capability-algebra.md with explicit composition laws and counter-examples; link it from L1.
2. Encode L1 transitions as DOT/PlantUML and render into Sphinx for auditability.
3. Write property tests that exercise non-associative composition orderings (fuzz the DAG scheduler with adversarial dependencies).
4. Gate merges on CI that builds both Meson and CMake, runs tests, and publishes Sphinx artefacts.
