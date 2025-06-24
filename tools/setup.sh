#!/usr/bin/env bash
# ===========================================================================
#  setup.sh — Dependency bootstrap for the Minix-1 C++23 revival
# ---------------------------------------------------------------------------
#  * Installs the Clang/LLVM 18 tool-chain (required)
#  * Pulls in analysis and doc tools (clang-tidy, cppcheck, doxygen, Sphinx…)
#  * Works on Ubuntu ≥ 22.04; warns but continues on partial failures
# ===========================================================================

set -euo pipefail

# ───────────────────────────── helpers ──────────────────────────────
opt_install() {
  # Attempt to install packages; warn instead of aborting on failure.
  if ! sudo apt-get install -y --no-install-recommends "$@"; then
    echo "setup.sh: warning: failed to install $*" >&2
    return 1
  fi
}

# ─────────────────────────── apt update ─────────────────────────────
if ! sudo apt-get update; then
  echo "setup.sh: warning: apt-get update failed; using stale package lists" >&2
fi

# ─────────────────────── clang / LLVM stack ─────────────────────────
# clang-18 is mandatory for building the project.  Abort if installation fails.
if ! opt_install clang-18 lld-18 lldb-18; then
  echo "setup.sh: error: clang-18 toolchain required" >&2
  exit 1
fi

# ───────────────────── core build / analysis pkgs ───────────────────
core_pkgs=(
  build-essential cmake nasm
  clang-tidy clang-format clang-tools clangd
  llvm llvm-dev libclang-dev libclang-cpp-dev
  cppcheck valgrind lcov strace gdb
  rustc cargo rustfmt
  g++ afl++ ninja-build
  doxygen graphviz
  python3-sphinx python3-breathe python3-sphinx-rtd-theme
  python3-pip
  libsodium-dev libsodium23 nlohmann-json3-dev
  qemu-system-x86 qemu-utils qemu-user
  tmux cloc cscope
  gcc-x86-64-linux-gnu g++-x86-64-linux-gnu binutils-x86-64-linux-gnu
)

opt_install "${core_pkgs[@]}"

# ───────────────────────── optional extras ──────────────────────────
# ack for fast searching
command -v ack >/dev/null 2>&1 || opt_install ack

# LibFuzzer headers, if archive provides them
opt_install libfuzzer-dev || true

# Python complexity metrics
python3 -m pip install --user --quiet lizard

# ─────────────────────────── summary ────────────────────────────────
if command -v clang++ >/dev/null 2>&1; then
  echo "Using clang: $(clang++ --version | head -n1)"
fi
if command -v ld.lld >/dev/null 2>&1; then
  echo "Using lld: $(ld.lld --version | head -n1)"
fi
if command -v lldb >/dev/null 2>&1; then
  echo "Using lldb: $(lldb --version | head -n1)"
fi

echo "setup.sh: dependency bootstrap complete."
