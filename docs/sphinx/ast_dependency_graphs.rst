AST Dependency Graphs
=====================

Overview
--------
The repository-wide AST dependency scan uses Tree-sitter front-ends where available
and regular-expression fallbacks for corner cases to enumerate ``#include`` and
import relationships across supported languages. Running the analyzer yields
canonical JSON artifacts consumed by this documentation and future automation.

Generation Workflow
-------------------
1. Ensure the Python environment contains ``tree_sitter_languages`` and a
   compatible ``tree_sitter`` runtime (the utility tolerates this by falling back
   to regex extraction when grammar quirks arise).
2. Execute ``python scripts/generate_ast_dependencies.py --root .`` from the
   repository root. The script emits:

   * ``docs/analysis/ast_dependency_graphs.json`` — per-language adjacency lists.
   * ``docs/analysis/ast_dependency_summary.json`` — aggregate statistics and
     top dependency emitters.

Language Highlights
-------------------
* **C (2,049 files / 5,311 edges)** — Dependency density is dominated by the
  architecture-specific ``dyn_syscalls.S`` tables for x86_64, i386, and ARM,
  alongside ``libc/dietlibc-xinim/t.c`` and ``libc/dietlibc-xinim/syscalls.h``.
  The shape indicates heavy preprocessor coupling in the libc surface.
* **C++ (585 files / 3,909 edges)** — ``include/xinim/stdlib_bridge.hpp`` drives
  the include fan-out, with command implementations such as ``src/commands/mkfs.cpp``
  also featuring prominently.
* **Python (2 files / 10 edges)** — The new analyzer and the Sphinx configuration
  are the only Python assets; the analyzer self-registers nine imports due to its
  modular helpers.
* **Bash (23 files / 7 edges)** — Dependency edges capture ``source`` invocations
  in the build pipeline scripts, led by toolchain bootstrap scripts and harmonized
  build entry points.
* **Lua / Perl** — Present but currently dependency-sparse in the tree.

Actionable Follow-ups
---------------------
* **Tame libc preprocessor pressure** by breaking up the generated syscall
  dispatch tables into smaller translation units and converging on common macros
  where possible; this should reduce the 782 combined edges from the ``dyn_syscalls``
  set and make rebuilds cheaper.
* **Refine C++ bridge layering** by reducing the include surface of
  ``stdlib_bridge.hpp``; identify narrow interfaces for callers (e.g., split into
  header-only utilities vs. POSIX shims) and update ``include/xinim/posix.hpp`` to
  pull only what each subsystem needs.
* **Extend language coverage** by adding Tree-sitter grammars for Lua and Perl or
  swapping to explicit module manifests so their edges become observable rather
  than empty.
* **Integrate CI checks** by wiring the analyzer into the documentation build so
  Sphinx surfaces drift in dependency hotspots alongside Doxygen/Breathe content.
