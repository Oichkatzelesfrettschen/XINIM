#!/usr/bin/env bash
# Cache official shell and standards sources for offline reference.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export XINIM_REPO_ROOT="$(dirname "${SCRIPT_DIR}")"
source "${SCRIPT_DIR}/xinim-env.sh"
xinim_ensure_project_dirs

DEST_DIR="${XINIM_REPO_ROOT}/data/external/posix"
mkdir -p "${DEST_DIR}"

fetch() {
    local url="$1"
    local name="$2"
    local path="${DEST_DIR}/${name}"
    curl -LfsS "${url}" -o "${path}"
}

fetch "https://www.opengroup.org/austin/" "austin_group_overview.html"
fetch "https://pubs.opengroup.org/onlinepubs/9699919799.2013edition/utilities/V3_chap02.html" \
    "posix_shell_command_language_2008.html"
fetch "https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap02.html" \
    "posix_conformance_2017.html"
fetch "https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap01.html" \
    "posix_shell_and_utilities_2017.html"
fetch "https://pubs.opengroup.org/onlinepubs/9799919799/xrat/V4_xcu_chap01.html" \
    "posix_issue8_shell_rationale.html"

sha256sum "${DEST_DIR}"/*.html > "${DEST_DIR}/SHA256SUMS"
