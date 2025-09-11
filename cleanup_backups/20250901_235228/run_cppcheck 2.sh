#!/bin/bash
## \file run_cppcheck.sh
## \brief Generate static analysis and code metrics reports.
##
## This script runs cppcheck, cloc, lizard, and cscope using project defaults.
## Results are stored under build/reports/ so CI can archive them.

set -euo pipefail

REPORT_DIR="build/reports"
mkdir -p "$REPORT_DIR"

# Run cppcheck with XML output for integration with review tools.
cppcheck --enable=all --std=c++23 --xml --xml-version=2 . 2>"$REPORT_DIR/cppcheck.xml"

# Count lines of code in JSON form.
cloc . --json --out="$REPORT_DIR/cloc.json"

# Generate complexity metrics.
lizard -o "$REPORT_DIR/lizard.json" .

# Build a cscope database for quick code search.
cscope -Rb -q -f "$REPORT_DIR/cscope.out"

printf 'Reports generated in %s\n' "$REPORT_DIR"

