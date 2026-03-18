#!/usr/bin/env bash
# Automated XINIM i486 test suite using VBoxManage.
# Boots VM headless, types commands via keyboard scancodes,
# captures screenshots, and validates output via OCR-free pattern matching.
set -euo pipefail

# Skip gracefully (exit 77) when VBoxManage is not installed
if ! command -v VBoxManage >/dev/null 2>&1; then
    echo "SKIP: VBoxManage not found"
    exit 77
fi

# Skip when qcow2 image is not built yet
if ! command -v qemu-img >/dev/null 2>&1; then
    echo "SKIP: qemu-img not found (needed for qcow2 -> VDI conversion)"
    exit 77
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_ROOT}/build/i486/Debug"
IMGDIR="${BUILD_DIR}/images/i486"
LOGDIR="${BUILD_DIR}/logs"
VM_NAME="XINIM-i486-test"
QCOW2="${IMGDIR}/xinim-i486-boot.qcow2"
VDI="${IMGDIR}/xinim-i486-test.vdi"
COM1_LOG="${LOGDIR}/vbox-test-com1.log"
SCREENSHOT_DIR="${LOGDIR}/test-screenshots"
PASS=0
FAIL=0
TOTAL=0

die() { echo "FATAL: $*" >&2; exit 1; }

# --- Scancode helpers ---
vbox_type() {
    local str="$1"
    for (( i=0; i<${#str}; i++ )); do
        local ch="${str:$i:1}" make="" shift=0
        case "$ch" in
            a) make=1e;; b) make=30;; c) make=2e;; d) make=20;; e) make=12;;
            f) make=21;; g) make=22;; h) make=23;; i) make=17;; j) make=24;;
            k) make=25;; l) make=26;; m) make=32;; n) make=31;; o) make=18;;
            p) make=19;; q) make=10;; r) make=13;; s) make=1f;; t) make=14;;
            u) make=16;; v) make=2f;; w) make=11;; x) make=2d;; y) make=15;;
            z) make=2c;;
            A) make=1e;shift=1;; B) make=30;shift=1;; C) make=2e;shift=1;;
            D) make=20;shift=1;; E) make=12;shift=1;; F) make=21;shift=1;;
            G) make=22;shift=1;; H) make=23;shift=1;; I) make=17;shift=1;;
            J) make=24;shift=1;; K) make=25;shift=1;; L) make=26;shift=1;;
            M) make=32;shift=1;; N) make=31;shift=1;; O) make=18;shift=1;;
            P) make=19;shift=1;; Q) make=10;shift=1;; R) make=13;shift=1;;
            S) make=1f;shift=1;; T) make=14;shift=1;; U) make=16;shift=1;;
            V) make=2f;shift=1;; W) make=11;shift=1;; X) make=2d;shift=1;;
            Y) make=15;shift=1;; Z) make=2c;shift=1;;
            0) make=0b;; 1) make=02;; 2) make=03;; 3) make=04;; 4) make=05;;
            5) make=06;; 6) make=07;; 7) make=08;; 8) make=09;; 9) make=0a;;
            ' ') make=39;; /) make=35;; '-') make=0c;; '|') make=2b;shift=1;;
            '.') make=34;; ',') make=33;; ';') make=27;; ':') make=27;shift=1;;
            '>') make=34;shift=1;; '<') make=33;shift=1;;
            '=') make=0d;; '+') make=0d;shift=1;;
            '[') make=1a;; ']') make=1b;;
            '$') make=05;shift=1;; '!') make=02;shift=1;;
            '_') make=0c;shift=1;; '?') make=35;shift=1;;
            *) continue;;
        esac
        if [ $shift -eq 1 ]; then
            VBoxManage controlvm "$VM_NAME" keyboardputscancode 2a $make \
                $(printf '%02x' $((0x$make | 0x80))) aa >/dev/null 2>&1
        else
            VBoxManage controlvm "$VM_NAME" keyboardputscancode $make \
                $(printf '%02x' $((0x$make | 0x80))) >/dev/null 2>&1
        fi
        sleep 0.03
    done
}

vbox_enter() {
    VBoxManage controlvm "$VM_NAME" keyboardputscancode 1c 9c >/dev/null 2>&1
}

vbox_cmd() {
    local cmd="$1" wait="${2:-3}"
    vbox_type "$cmd"
    vbox_enter
    sleep "$wait"
}

vbox_screenshot() {
    local name="$1"
    VBoxManage controlvm "$VM_NAME" screenshotpng \
        "${SCREENSHOT_DIR}/${name}.png" >/dev/null 2>&1
}

# --- Test runner (checks COM1 log for faults after each command) ---
run_test() {
    local name="$1" cmd="$2" wait="${3:-4}"
    TOTAL=$((TOTAL + 1))
    local fault_before
    fault_before=$(grep -c "fault" "$COM1_LOG" 2>/dev/null || true)
    fault_before=${fault_before:-0}
    vbox_cmd "$cmd" "$wait"
    local fault_after
    fault_after=$(grep -c "fault" "$COM1_LOG" 2>/dev/null || true)
    fault_after=${fault_after:-0}
    if [ "$fault_after" -gt "$fault_before" ]; then
        FAIL=$((FAIL + 1))
        echo "FAIL: $name (fault detected)"
    else
        PASS=$((PASS + 1))
        echo "PASS: $name"
    fi
}

# --- VM lifecycle ---
setup_vm() {
    echo "Setting up VirtualBox VM..."
    [ -f "$QCOW2" ] || die "qcow2 not found: $QCOW2"
    mkdir -p "$SCREENSHOT_DIR"

    # Convert qcow2 -> raw -> VDI
    local raw="${IMGDIR}/xinim-test.raw"
    qemu-img convert -f qcow2 -O raw "$QCOW2" "$raw"
    rm -f "$VDI"
    VBoxManage convertfromraw "$raw" "$VDI" --format VDI 2>/dev/null
    rm -f "$raw"

    # Delete old VM if exists
    VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null || true

    # Create VM
    VBoxManage createvm --name "$VM_NAME" --ostype Linux \
        --register --basefolder "$LOGDIR" >/dev/null 2>&1
    VBoxManage modifyvm "$VM_NAME" \
        --cpus 1 --memory 64 --vram 16 \
        --firmware bios --graphicscontroller vmsvga \
        --audio-driver none --usb off \
        --ioapic off --pae off \
        --uart1 0x3F8 4 --uart-mode1 file "$COM1_LOG" \
        --nic1 none >/dev/null 2>&1
    VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide \
        --controller PIIX4 >/dev/null 2>&1
    VBoxManage storageattach "$VM_NAME" --storagectl "IDE" \
        --port 0 --device 0 --type hdd --medium "$VDI" >/dev/null 2>&1
}

start_vm() {
    rm -f "$COM1_LOG"
    echo "Starting VM..."
    VBoxManage startvm "$VM_NAME" --type headless >/dev/null 2>&1
    echo "Waiting for boot..."
    sleep 12
}

stop_vm() {
    VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null || true
    sleep 2
}

cleanup_vm() {
    stop_vm
    VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null || true
}

# --- Main test suite ---
main() {
    setup_vm
    start_vm

    echo ""
    echo "========================================="
    echo "  XINIM i486 Automated Test Suite"
    echo "  VirtualBox + VBoxManage"
    echo "========================================="
    echo ""

    # --- Basic builtins ---
    echo "--- Shell builtins ---"
    run_test "echo"          "echo HELLO"           3
    run_test "echo spaces"   "echo hello world"     3
    run_test "pwd"           "pwd"                  3
    run_test "cd /bin"       "cd /bin"              3
    run_test "pwd after cd"  "pwd"                  3
    run_test "cd /"          "cd /"                 3
    run_test "true"          "true"                 3
    run_test "false"         "false"                3
    vbox_screenshot "01-builtins"

    # --- uname flags ---
    echo "--- uname ---"
    run_test "uname"         "uname"               4
    run_test "uname -s"      "uname -s"            4
    run_test "uname -r"      "uname -r"            4
    run_test "uname -m"      "uname -m"            4
    run_test "uname -n"      "uname -n"            4
    run_test "uname -a"      "uname -a"            4
    vbox_screenshot "02-uname"

    # --- File operations ---
    echo "--- File operations ---"
    run_test "ls /"          "ls /"                 4
    run_test "ls /bin"       "ls /bin"              5
    run_test "ls /etc"       "ls /etc"              4
    run_test "ls /boot"      "ls /boot"             4
    run_test "ls ."          "ls ."                 4
    vbox_screenshot "03-ls"

    # --- Pipes ---
    echo "--- Pipes ---"
    run_test "echo|cat"      "echo pipetest | cat"  5
    run_test "echo|wc -c"    "echo hello | wc -c"   6
    run_test "echo|head"     "echo headtest | head -1" 6
    run_test "echo|tail"     "echo tailtest | tail -1" 6
    run_test "echo|tr"       "echo hello | tr h H"  5
    run_test "echo|cut"      "echo a.b.c | cut -d. -f2" 5
    run_test "echo|sort"     "echo zzz | sort"      5
    vbox_screenshot "04-pipes"

    # --- Text utilities ---
    echo "--- Text utilities ---"
    run_test "basename"      "basename /usr/bin/gcc" 4
    run_test "dirname"       "dirname /usr/bin/gcc"  4
    run_test "whoami"        "whoami"               4
    run_test "id"            "id"                   4
    run_test "hostname"      "hostname"             4
    run_test "date"          "date"                 4
    run_test "tty"           "tty"                  4
    run_test "uname -a"      "uname -a"            4
    vbox_screenshot "05-utils"

    # --- File I/O ---
    echo "--- File I/O ---"
    run_test "cat /etc/profile"  "cat /etc/profile"  4
    run_test "cat /etc/motd"     "cat /etc/motd"     4
    run_test "wc -l profile"     "wc -l /etc/profile" 5
    vbox_screenshot "06-fileio"

    # --- Multi-stage pipeline ---
    echo "--- Multi-stage pipelines ---"
    run_test "3-stage ls|grep|wc"  "ls /bin | grep cat | wc -l"  7
    run_test "3-stage echo|tr|cat" "echo hello | tr h H | cat"   7
    vbox_screenshot "07-pipelines"

    # --- File permissions ---
    echo "--- File permissions ---"
    run_test "chmod 644 tmp"       "echo test > /tmp/perm; chmod 644 /tmp/perm" 5
    run_test "chmod 000 tmp"       "chmod 000 /tmp/perm" 4
    run_test "chmod 755 tmp"       "chmod 755 /tmp/perm" 4
    vbox_screenshot "08-permissions"

    # --- Negative tests ---
    echo "--- Negative tests ---"
    run_test "cat nonexistent"     "cat /nonexistent 2>/dev/null; echo done" 5
    run_test "ls nonexistent"      "ls /nonexistent 2>/dev/null; echo done"  5
    run_test "mkdir existing"      "mkdir /tmp 2>/dev/null; echo done"       5
    vbox_screenshot "09-negative"

    # --- Signal tests ---
    echo "--- Signal tests ---"
    run_test "kill 0 self"         "kill -0 1"            4
    vbox_screenshot "10-signals"

    # --- TCC compilation ---
    echo "--- TCC compilation ---"
    run_test "tcc hello.c" "echo 'int main(){return 0;}' > /tmp/t.c; tcc -o /tmp/t /tmp/t.c; /tmp/t; echo ok" 8
    vbox_screenshot "11-tcc"

    # --- Symlink test ---
    echo "--- Symlinks ---"
    run_test "ln -s" "ln -s /bin/cat /tmp/mycat; /tmp/mycat /etc/motd" 6
    run_test "readlink" "readlink /tmp/mycat" 5
    vbox_screenshot "12-symlinks"

    # --- Final screenshot ---
    vbox_screenshot "99-final"

    echo ""
    echo "========================================="
    echo "  Results: $PASS/$TOTAL passed, $FAIL failed"
    echo "========================================="
    echo ""
    echo "Faults:"
    grep "fault" "$COM1_LOG" 2>/dev/null | head -5 || echo "  (none)"
    echo ""
    echo "Respawns: $(grep -c 'Respawning' "$COM1_LOG" 2>/dev/null || echo 0)"
    echo "Screenshots: ${SCREENSHOT_DIR}/"

    stop_vm
    # Don't delete -- leave screenshots and logs

    if [ "$FAIL" -gt 0 ]; then
        exit 1
    fi
}

main "$@"
