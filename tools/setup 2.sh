#!/usr/bin/env bash
# ===========================================================================
#  setup.sh — Dependency bootstrap for the Minix-1 C++23 revival
# ---------------------------------------------------------------------------
#  * Installs Clang/LLVM tool-chain (prefers 18, falls back to 20, then distro)
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
if ! opt_install clang-18 lld-18 lldb-18; then
  # Try the official LLVM script for clang-20
  if curl -fsSL https://apt.llvm.org/llvm.sh | sudo bash -s -- 20; then
    opt_install clang-20 lld-20 lldb-20
  else
    # Fall back to distro default
    opt_install clang lld lldb
  fi
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
