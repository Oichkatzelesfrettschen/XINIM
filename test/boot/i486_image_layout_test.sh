#!/bin/sh
# Compatibility wrapper for the generic 32-bit image layout test.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export XINIM_BOOT_LANE_NAME="${XINIM_BOOT_LANE_NAME:-i486}"
exec "${SCRIPT_DIR}/x86_32_image_layout_test.sh"
