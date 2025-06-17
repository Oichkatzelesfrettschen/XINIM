#!/bin/sh
## \file pre-commit-clang-format.sh
## \brief Git pre-commit hook to format staged C++ files with clang-format.
##
## This hook formats any staged `.cpp` or `.hpp` files using clang-format
## and re-adds them to the commit. It prevents committing improperly
## formatted code and keeps style consistent across contributions.
##
## Usage:
##     ln -s ../../tools/pre-commit-clang-format.sh .git/hooks/pre-commit
##
## \author Auto-generated

FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp)$')
if [ -n "$FILES" ]; then
    echo "$FILES" | xargs clang-format -i
    echo "$FILES" | xargs git add
fi

exit 0
