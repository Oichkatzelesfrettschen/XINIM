# Architecture Overview

XINIM revisits the original MINIX 1 microkernel through a modern C++23 lens.  The
system is organised into a stack of well defined layers that separate abstract
mathematics from concrete implementation.  Each layer exposes a carefully scoped
interface and is documented using Doxygen so that Sphinx, via the Breathe
extension, can weave the narrative into the developer manual.

## Layered Design

1. **L0 – Governing Mathematical Model**  \
   Formal definitions of security labels and a capability algebra grounded in
   octonions.
2. **L1 – Abstract Kernel Contract**  \
   State machines define capability lifecycles, message channels and scheduling
   invariants.
3. **L2 – Algorithmic Realisation**  \
   Data structures such as `lattice::Channel` and `sched::Scheduler` implement
   the contract, using a dependency DAG to drive scheduling.
4. **L3 – C++23 Skeleton**  \
   Namespaces, classes and concepts reside in header files, all annotated with
   Doxygen comments consumed by Breathe.
5. **L4 – Tool Chain**  \
   Meson and CMake orchestrate builds with Clang 18 while Doxygen and Sphinx
   generate the reference manuals.

For a comprehensive and continually updated discussion, consult the Sphinx
manual’s [architecture chapter](sphinx/architecture.rst).

```bash
# Regenerate the full documentation set
cd docs

doxygen Doxyfile
sphinx-build -b html sphinx sphinx/html
```

Open `docs/sphinx/html/index.html` in a browser to explore the rendered site.
