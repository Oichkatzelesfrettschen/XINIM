#!/bin/bash

# Reconcile duplicate files in XINIM project
# This script identifies and helps resolve duplicate files with " 2" suffix

echo "=== XINIM Duplicate File Reconciliation ==="
echo

# Count duplicates
DUPLICATE_COUNT=$(find . -name "* 2.*" -type f | grep -v build | wc -l)
echo "Found $DUPLICATE_COUNT duplicate files with ' 2' suffix"
echo

# Create backup directory
BACKUP_DIR="duplicate_backup_$(date +%Y%m%d_%H%M%S)"
echo "Creating backup directory: $BACKUP_DIR"
mkdir -p "$BACKUP_DIR"

# Function to compare and reconcile files
reconcile_file() {
    local dup_file="$1"
    local orig_file="${dup_file// 2/}"
    
    if [ -f "$orig_file" ]; then
        # Both files exist - compare them
        if diff -q "$orig_file" "$dup_file" > /dev/null 2>&1; then
            echo "  ✓ Identical: $dup_file (safe to remove)"
            mv "$dup_file" "$BACKUP_DIR/"
        else
            echo "  ⚠ Different: $dup_file vs $orig_file"
            echo "    Size: $(wc -c < "$dup_file") vs $(wc -c < "$orig_file") bytes"
            # Keep both for manual review
        fi
    else
        echo "  ! No original for: $dup_file (may be unique)"
    fi
}

# Process each duplicate file
echo "Processing duplicates..."
while IFS= read -r file; do
    reconcile_file "$file"
done < <(find . -name "* 2.*" -type f | grep -v build | grep -v "$BACKUP_DIR")

echo
echo "=== Summary ==="
echo "Backed up files to: $BACKUP_DIR"
echo "Files requiring manual review:"
find . -name "* 2.*" -type f | grep -v build | grep -v "$BACKUP_DIR" | head -20

echo
echo "To complete reconciliation:"
echo "1. Review remaining ' 2' files manually"
echo "2. Run: rm -rf '$BACKUP_DIR' (after verification)"
echo "3. Update CMakeLists.txt files if needed"