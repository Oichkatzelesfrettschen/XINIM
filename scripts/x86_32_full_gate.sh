#!/usr/bin/env bash
# Run the serial 32-bit XINIM build/boot gate with host-tool checks.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

lanes=(i686 i586 i486)

export XINIM_QEMU_CMD_TIMEOUT="${XINIM_QEMU_CMD_TIMEOUT:-30}"
export XINIM_QEMU_PROMPT_SETTLE_TIMEOUT="${XINIM_QEMU_PROMPT_SETTLE_TIMEOUT:-1.5}"

run_step() {
  local label="$1"
  shift
  printf "\n==> %s\n" "$label"
  "$@"
}

run_step "host tooling audit" bash scripts/x86_32_tooling_audit.sh

run_step "python syntax" python3 -m py_compile \
  scripts/qemu_x86_32_debug.py \
  scripts/create_i486_boot_disk.py \
  scripts/qemu_matrix.py \
  test/boot/x86_32_shell_test.py \
  test/boot/x86_32_persist_test.py \
  test/boot/x86_32_ext2_mutation_test.py \
  test/boot/x86_32_enhanced_test.py

run_step "shell syntax" bash -n \
  scripts/qemu_i486.sh \
  scripts/build.sh \
  scripts/build_i486.sh \
  scripts/x86_32_tooling_audit.sh \
  scripts/x86_32_full_gate.sh

for lane in "${lanes[@]}"; do
  build_dir="build/${lane}/Debug"
  if [[ ! -d "$build_dir" ]]; then
    printf "\n==> skip %s: missing build dir %s\n" "$lane" "$build_dir"
    continue
  fi
  run_step "build ${lane}" cmake --build "$build_dir" --target all
  run_step "ctest ${lane}" ctest --test-dir "$build_dir" --output-on-failure -E 'vbox'
done

image_count=0
while IFS= read -r -d '' image; do
  image_count=$((image_count + 1))
  run_step "qemu-img check ${image}" qemu-img check "$image"
done < <(find build -type f \( -name '*.vmdk' -o -name '*.qcow2' \) -print0 | sort -z)

if [[ "$image_count" -eq 0 ]]; then
  printf "\n==> no VMDK/qcow2 images found under build/\n"
fi

run_step "git whitespace check" git diff --check
