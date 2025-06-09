# AGENTS instructions

All C++ source and header files modified by the agent must be fully documented using Doxygen style comments.
Use modern C++23 constructs where reasonable.
Run `clang-format -i` on any changed C++ files before committing.
All files the agent edits must be decomposed, flattened, unrolled, and refactored
into modern idiomatic constructs regardless of language. Every function and class
needs granular Doxygen comments.
Documentation must integrate with Sphinx using the Breathe extension so
that Read the Docs can process it.
