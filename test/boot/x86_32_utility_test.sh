#!/bin/sh
# XINIM i486 Utility Smoke Test
# Run inside the XINIM mksh shell to verify all Tier 1/2/3 utilities work.
# Exit code: 0 if all pass, 1 if any fail.

PASS=0
FAIL=0

check() {
    name="$1"
    got="$2"
    expected="$3"
    if [ "$got" = "$expected" ]; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (got '$got', expected '$expected')"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== XINIM Utility Smoke Test ==="

# Tier 1: Shell essentials
check "echo" "$(echo hello world)" "hello world"
check "true exit" "$(true; echo $?)" "0"
check "pwd" "$(pwd)" "/"
check "basename" "$(basename /bin/mksh)" "mksh"
check "basename suffix" "$(basename /bin/mksh .ksh)" "mksh"
check "dirname" "$(dirname /bin/mksh)" "/bin"
check "test -f" "$(test -f /bin/mksh; echo $?)" "0"
check "test -d" "$(test -d /bin; echo $?)" "0"
check "printf" "$(printf '%s %d' hello 42)" "hello 42"

# Tier 2: Text processing
check "wc -l" "$(echo -e 'a\nb\nc' | wc -l)" " 3"
check "head -n 1" "$(echo -e 'first\nsecond' | head -n 1)" "first"
check "sort" "$(echo -e 'b\na\nc' | sort | head -n 1)" "a"
check "uniq" "$(echo -e 'a\na\nb' | uniq | wc -l)" " 2"
check "cut" "$(echo 'a:b:c' | cut -d : -f 2)" "b"
check "tr" "$(echo hello | tr h H)" "Hello"
check "grep" "$(echo hello | grep hello)" "hello"
check "grep -v" "$(echo -e 'yes\nno' | grep -v no)" "yes"

# Tier 3: System utilities
check "uname -s" "$(uname -s)" "XINIM"
check "uname -m" "$(uname -m)" "i486"
check "id uid" "$(id | grep uid=0)" "uid=0(root) gid=0(root) euid=0 egid=0"
check "sleep" "$(sleep 0; echo ok)" "ok"
check "nproc" "$(nproc)" "1"
check "seq" "$(seq 3 | tail -n 1)" "3"
check "expr" "$(expr 2 + 3)" "5"

echo "=== $PASS passed, $FAIL failed ==="
exit $FAIL
