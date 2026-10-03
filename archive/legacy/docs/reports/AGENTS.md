# AGENTS instructions

The project requires a C++23 capable compiler. Clang 18 and the matching LLVM 18 suite (including lld and lldb) are the preferred minimum. Install the entire tool suite before building or running tests.

All C++ source and header files modified by the agent must be fully documented using Doxygen style comments.
Use modern C++23 constructs where reasonable.
Run `clang-format -i` on any changed C++ files before committing.
All files the agent edits must be decomposed, flattened, unrolled, and refactored
into modern idiomatic constructs regardless of language. Every function and class
needs granular Doxygen comments.
Documentation must integrate with Sphinx using the Breathe extension so
that Read the Docs can process it.
