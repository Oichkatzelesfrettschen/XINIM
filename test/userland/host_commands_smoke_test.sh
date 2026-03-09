#!/bin/sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
HOSTBIN="${XINIM_HOSTBIN:-${PROJECT_ROOT}/build/x86_64/Debug/hostbin}"

require_bin() {
    if [ ! -x "${HOSTBIN}/$1" ]; then
        echo "SKIP: missing hosted command ${HOSTBIN}/$1"
        exit 77
    fi
}

for cmd in echo pwd true false mkdir rm touch cp ls cat wc ln chmod basename head env sleep rev tee cut rmdir sum tr date comm sync cmp uniq od kill chown mv df; do
    require_bin "${cmd}"
done

TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

OUT="$("${HOSTBIN}/echo" hello world)"
[ "${OUT}" = "hello world" ] || {
    echo "FAIL: echo output mismatch"
    exit 1
}

(
    cd "${TMPDIR}"

    PWD_OUT="$("${HOSTBIN}/pwd")"
    [ "${PWD_OUT}" = "${TMPDIR}" ] || {
        echo "FAIL: pwd output mismatch"
        exit 1
    }

    "${HOSTBIN}/mkdir" -p alpha/beta
    "${HOSTBIN}/touch" alpha/beta/file.txt
    printf 'sample\n' > seed.txt
    "${HOSTBIN}/cp" seed.txt alpha/beta/copied.txt

    LS_OUT="$("${HOSTBIN}/ls" alpha/beta)"
    printf '%s\n' "${LS_OUT}" | grep -q "file.txt" || {
        echo "FAIL: ls missing file.txt"
        exit 1
    }
    printf '%s\n' "${LS_OUT}" | grep -q "copied.txt" || {
        echo "FAIL: ls missing copied.txt"
        exit 1
    }

    CAT_OUT="$("${HOSTBIN}/cat" alpha/beta/copied.txt)"
    [ "${CAT_OUT}" = "sample" ] || {
        echo "FAIL: cat output mismatch"
        exit 1
    }

    WC_OUT="$("${HOSTBIN}/wc" alpha/beta/copied.txt)"
    printf '%s\n' "${WC_OUT}" | grep -Eq '[[:space:]]+1[[:space:]]+1[[:space:]]+7[[:space:]]+alpha/beta/copied.txt$' || {
        echo "FAIL: wc output mismatch"
        exit 1
    }

    "${HOSTBIN}/ln" alpha/beta/copied.txt alpha/beta/copied.hard
    [ -f alpha/beta/copied.hard ] || {
        echo "FAIL: ln did not create hard link"
        exit 1
    }
    [ "$(stat -c '%i' alpha/beta/copied.txt)" = "$(stat -c '%i' alpha/beta/copied.hard)" ] || {
        echo "FAIL: hard link inode mismatch"
        exit 1
    }

    "${HOSTBIN}/ln" -s copied.txt alpha/beta/copied.sym
    [ -L alpha/beta/copied.sym ] || {
        echo "FAIL: ln did not create symlink"
        exit 1
    }
    [ "$("${HOSTBIN}/cat" alpha/beta/copied.sym)" = "sample" ] || {
        echo "FAIL: symlink target read mismatch"
        exit 1
    }

    "${HOSTBIN}/chmod" 600 alpha/beta/copied.txt
    [ "$(stat -c '%a' alpha/beta/copied.txt)" = "600" ] || {
        echo "FAIL: chmod did not update permissions"
        exit 1
    }

    BASENAME_OUT="$("${HOSTBIN}/basename" /tmp/example/path.txt .txt)"
    [ "${BASENAME_OUT}" = "path" ] || {
        echo "FAIL: basename output mismatch"
        exit 1
    }

    printf 'first\nsecond\nthird\n' > alpha/beta/multi.txt
    HEAD_OUT="$("${HOSTBIN}/head" -n 2 alpha/beta/multi.txt)"
    EXPECTED_HEAD="$(printf 'first\nsecond')"
    [ "${HEAD_OUT}" = "${EXPECTED_HEAD}" ] || {
        echo "FAIL: head output mismatch"
        exit 1
    }

    ENV_OUT="$("${HOSTBIN}/env" -i FOO=bar BAR=baz)"
    printf '%s\n' "${ENV_OUT}" | grep -q '^FOO=bar$' || {
        echo "FAIL: env missing FOO=bar"
        exit 1
    }
    printf '%s\n' "${ENV_OUT}" | grep -q '^BAR=baz$' || {
        echo "FAIL: env missing BAR=baz"
        exit 1
    }

    "${HOSTBIN}/sleep" 0
    if "${HOSTBIN}/sleep" nope 2>/dev/null; then
        echo "FAIL: sleep accepted invalid argument"
        exit 1
    fi

    REV_OUT="$(printf 'abc\nxash\n' | "${HOSTBIN}/rev")"
    EXPECTED_REV="$(printf 'cba\nhsax')"
    [ "${REV_OUT}" = "${EXPECTED_REV}" ] || {
        echo "FAIL: rev output mismatch"
        exit 1
    }

    TEE_STDOUT="$(printf 'left\nright\n' | "${HOSTBIN}/tee" alpha/beta/tee.txt)"
    EXPECTED_TEE="$(printf 'left\nright')"
    [ "${TEE_STDOUT}" = "${EXPECTED_TEE}" ] || {
        echo "FAIL: tee stdout mismatch"
        exit 1
    }
    [ "$("${HOSTBIN}/cat" alpha/beta/tee.txt)" = "${EXPECTED_TEE}" ] || {
        echo "FAIL: tee file contents mismatch"
        exit 1
    }

    printf 'a:b:c\n1:2:3\n' > alpha/beta/cut.txt
    CUT_OUT="$("${HOSTBIN}/cut" -d : -f 2 alpha/beta/cut.txt)"
    EXPECTED_CUT="$(printf 'b\n2')"
    [ "${CUT_OUT}" = "${EXPECTED_CUT}" ] || {
        echo "FAIL: cut output mismatch"
        exit 1
    }

    "${HOSTBIN}/mkdir" empty
    "${HOSTBIN}/rmdir" empty
    [ ! -e empty ] || {
        echo "FAIL: rmdir did not remove empty directory"
        exit 1
    }

    printf 'A\n' > alpha/beta/sum.txt
    SUM_OUT="$("${HOSTBIN}/sum" alpha/beta/sum.txt)"
    [ "${SUM_OUT}" = "32810 1 alpha/beta/sum.txt" ] || {
        echo "FAIL: sum output mismatch"
        exit 1
    }

    TR_OUT="$(printf 'abc123\n' | "${HOSTBIN}/tr" a-z A-Z)"
    [ "${TR_OUT}" = "ABC123" ] || {
        echo "FAIL: tr output mismatch"
        exit 1
    }

    DATE_OUT="$("${HOSTBIN}/date")"
    [ -n "${DATE_OUT}" ] || {
        echo "FAIL: date output empty"
        exit 1
    }

    printf 'apple\nbanana\npear\n' > alpha/left.txt
    printf 'banana\npear\nplum\n' > alpha/right.txt
    COMM_OUT="$("${HOSTBIN}/comm" alpha/left.txt alpha/right.txt)"
    EXPECTED_COMM="$(printf 'apple\n\t\tbanana\n\t\tpear\n\tplum')"
    [ "${COMM_OUT}" = "${EXPECTED_COMM}" ] || {
        echo "FAIL: comm output mismatch"
        exit 1
    }

    "${HOSTBIN}/sync"

    printf 'same\ncontent\n' > alpha/cmp-a.txt
    printf 'same\ncontent\n' > alpha/cmp-b.txt
    "${HOSTBIN}/cmp" alpha/cmp-a.txt alpha/cmp-b.txt
    printf 'same\nchanged\n' > alpha/cmp-b.txt
    if "${HOSTBIN}/cmp" -s alpha/cmp-a.txt alpha/cmp-b.txt; then
        echo "FAIL: cmp -s returned success for different files"
        exit 1
    fi

    printf 'dup\ndup\nsolo\n' > alpha/uniq.txt
    UNIQ_OUT="$("${HOSTBIN}/uniq" alpha/uniq.txt)"
    EXPECTED_UNIQ="$(printf 'dup\nsolo')"
    [ "${UNIQ_OUT}" = "${EXPECTED_UNIQ}" ] || {
        echo "FAIL: uniq output mismatch"
        exit 1
    }

    OD_OUT="$(printf 'A' | "${HOSTBIN}/od" -c)"
    printf '%s\n' "${OD_OUT}" | grep -q 'A' || {
        echo "FAIL: od output missing character dump"
        exit 1
    }

    "${HOSTBIN}/rm" -r alpha
    [ ! -e alpha ] || {
        echo "FAIL: rm did not remove directory tree"
        exit 1
    }

    "${HOSTBIN}/true"
    if "${HOSTBIN}/false"; then
        echo "FAIL: false returned success"
        exit 1
    fi
)

echo "PASS: hosted commands smoke test"
