#!/usr/bin/env python3
"""VirtualBox VM lifecycle manager for XINIM i486.

WHY: cmake/VBoxLane.cmake creates CMake custom targets (vbox_setup_i486,
     vbox_run_i486, vbox_test_i486, vbox_stop_i486, etc.) that invoke this
     script.  Centralising all VBoxManage calls here keeps CMakeLists.txt
     clean and makes each operation independently testable.

WHAT: Commands:
    setup      -- convert qcow2 -> VDI, create/register VM, attach disk
    start      -- start VM headless
    stop       -- power off VM
    teardown   -- power off + unregister + delete
    screenshot -- take PNG screenshot (for CI artefacts)
    test       -- run the existing test suite (scripts/test_i486_vbox.sh)
    pgo-collect -- boot PGO-instrumented kernel, wait for XNPGO_END in serial
                   log, then power off (profile extraction is a separate step)
    status     -- print VM power state

HOW:
    python3 scripts/vbox_i486.py --build-dir build/i486/Debug setup
    python3 scripts/vbox_i486.py --build-dir build/i486/Debug start
    python3 scripts/vbox_i486.py --build-dir build/i486/Debug test
    python3 scripts/vbox_i486.py --build-dir build/i486/Debug stop
    python3 scripts/vbox_i486.py --build-dir build/i486/Debug teardown
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


BOOT_WAIT_S = 20          # seconds to wait after startvm before issuing commands
PGO_TIMEOUT_S = 300       # maximum seconds to wait for XNPGO_END after boot


def vm_name(lane: str) -> str:
    """Derive the VirtualBox VM name from the CPU lane."""
    return f"XINIM-{lane}"


def vbm(*args: str, check: bool = True, capture: bool = False) -> str:
    """Run a VBoxManage command.  Returns stdout when capture=True."""
    cmd = ["VBoxManage"] + list(args)
    if capture:
        r = subprocess.run(cmd, check=check, capture_output=True, text=True)
        return r.stdout
    subprocess.run(cmd, check=check)
    return ""


def vm_exists(name: str) -> bool:
    try:
        out = vbm("showvminfo", name, "--machinereadable",
                  check=False, capture=True)
        return "VMState=" in out
    except Exception:
        return False


def vm_running(name: str) -> bool:
    try:
        out = vbm("showvminfo", name, "--machinereadable",
                  check=False, capture=True)
        return 'VMState="running"' in out
    except Exception:
        return False


def require_vboxmanage() -> None:
    if not shutil.which("VBoxManage"):
        sys.exit("error: VBoxManage not found in PATH -- install VirtualBox")


def require_qemu_img() -> None:
    if not shutil.which("qemu-img"):
        sys.exit("error: qemu-img not found in PATH (needed for qcow2 -> VDI)")


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_setup(args: argparse.Namespace) -> None:
    require_vboxmanage()
    require_qemu_img()

    name = vm_name(args.lane)
    imgdir = Path(args.build_dir) / "images" / args.lane
    logdir = Path(args.build_dir) / "logs"
    qcow2 = imgdir / f"xinim-{args.lane}-boot.qcow2"
    vdi = imgdir / f"{name}.vdi"
    com1_log = logdir / "vbox-com1.log"

    if not qcow2.is_file():
        sys.exit(f"error: boot image not found: {qcow2}\n"
                 f"       Build the 'xinim_{args.lane}_image' CMake target first.")

    logdir.mkdir(parents=True, exist_ok=True)

    # Convert qcow2 -> raw -> VDI
    raw = imgdir / f"{name}-tmp.raw"
    print(f"Converting {qcow2.name} -> VDI ...")
    subprocess.run(["qemu-img", "convert", "-f", "qcow2", "-O", "raw",
                    str(qcow2), str(raw)], check=True)
    if vdi.exists():
        vdi.unlink()
    vbm("convertfromraw", str(raw), str(vdi), "--format", "VDI")
    raw.unlink(missing_ok=True)
    print(f"VDI: {vdi}")

    # i686 needs IOAPIC enabled; i486 uses legacy PIC only
    ioapic_flag = "on" if args.lane == "i686" else "off"
    memory_mb = "64" if args.lane == "i486" else "128"

    # Tear down stale VM if it exists
    if vm_exists(name):
        if vm_running(name):
            vbm("controlvm", name, "poweroff", check=False)
            time.sleep(2)
        vbm("unregistervm", name, "--delete", check=False)
        time.sleep(1)

    # Create VM
    vbm("createvm", "--name", name, "--ostype", "Linux",
        "--register", "--basefolder", str(logdir))
    vbm("modifyvm", name,
        "--cpus", "1",
        "--memory", memory_mb,
        "--vram", "16",
        "--firmware", "bios",
        "--graphicscontroller", "vmsvga",
        "--audio-driver", "none",
        "--usb", "off",
        "--ioapic", ioapic_flag,
        "--pae", "off",
        "--uart1", "0x3F8", "4",
        "--uart-mode1", "file", str(com1_log),
        "--nic1", "none")
    vbm("storagectl", name, "--name", "IDE", "--add", "ide",
        "--controller", "PIIX4")
    vbm("storageattach", name, "--storagectl", "IDE",
        "--port", "0", "--device", "0", "--type", "hdd", "--medium", str(vdi))
    print(f"VM '{name}' registered.  COM1 -> {com1_log}")


def cmd_start(args: argparse.Namespace) -> None:
    require_vboxmanage()
    name = vm_name(args.lane)
    if not vm_exists(name):
        sys.exit(f"error: VM '{name}' not found -- run 'setup' first")
    if vm_running(name):
        print(f"VM '{name}' is already running")
        return
    vbm("startvm", name, "--type", "headless")
    print(f"VM '{name}' started headless.  Waiting {BOOT_WAIT_S}s for boot ...")
    time.sleep(BOOT_WAIT_S)


def cmd_stop(args: argparse.Namespace) -> None:
    require_vboxmanage()
    name = vm_name(args.lane)
    if not vm_running(name):
        print(f"VM '{name}' is not running")
        return
    vbm("controlvm", name, "poweroff", check=False)
    time.sleep(2)
    print(f"VM '{name}' stopped")


def cmd_teardown(args: argparse.Namespace) -> None:
    require_vboxmanage()
    name = vm_name(args.lane)
    if vm_running(name):
        vbm("controlvm", name, "poweroff", check=False)
        time.sleep(2)
    if vm_exists(name):
        vbm("unregistervm", name, "--delete", check=False)
        print(f"VM '{name}' removed")
    else:
        print(f"VM '{name}' does not exist")


def cmd_screenshot(args: argparse.Namespace) -> None:
    require_vboxmanage()
    name = vm_name(args.lane)
    logdir = Path(args.build_dir) / "logs"
    outdir = logdir / "vbox-screenshots"
    outdir.mkdir(parents=True, exist_ok=True)
    snap_name = args.name or "snapshot"
    png = outdir / f"{snap_name}.png"
    vbm("controlvm", name, "screenshotpng", str(png))
    print(f"Screenshot: {png}")


def cmd_status(args: argparse.Namespace) -> None:
    require_vboxmanage()
    name = vm_name(args.lane)
    if not vm_exists(name):
        print(f"VM '{name}': not registered")
        return
    out = vbm("showvminfo", name, "--machinereadable", capture=True)
    for line in out.splitlines():
        if line.startswith("VMState="):
            print(f"VM '{name}': {line}")
            return


def cmd_test(args: argparse.Namespace) -> None:
    """Run the existing bash test suite against the running VM."""
    require_vboxmanage()
    script = Path(args.project_root) / "scripts" / "test_i486_vbox.sh"
    if not script.is_file():
        sys.exit(f"error: test script not found: {script}")
    env = dict(os.environ)
    env["BUILD_DIR"] = str(args.build_dir)
    env["XINIM_LANE"] = args.lane
    subprocess.run(["bash", str(script)], env=env, check=True)


def cmd_pgo_collect(args: argparse.Namespace) -> None:
    """Boot the PGO-instrumented kernel; wait for XNPGO_END in COM1 log."""
    require_vboxmanage()
    name = vm_name(args.lane)
    logdir = Path(args.build_dir) / "logs"
    com1_log = logdir / "vbox-pgo-com1.log"

    if not vm_exists(name):
        sys.exit(f"error: VM '{name}' not found -- run 'setup' first with "
                 "the PGO-instrumented disk image attached")

    # Clear old log
    if com1_log.exists():
        com1_log.unlink()
    # Update COM1 log path to PGO-specific file
    if vm_running(name):
        vbm("controlvm", name, "poweroff", check=False)
        time.sleep(2)
    vbm("modifyvm", name,
        "--uart-mode1", "file", str(com1_log))

    print("Starting VM for PGO profile collection ...")
    vbm("startvm", name, "--type", "headless")

    deadline = time.monotonic() + PGO_TIMEOUT_S
    print(f"Waiting up to {PGO_TIMEOUT_S}s for XNPGO_END in {com1_log} ...")
    while time.monotonic() < deadline:
        time.sleep(5)
        if com1_log.is_file():
            content = com1_log.read_text(errors="replace")
            if "XNPGO_END" in content:
                print("XNPGO_END detected -- profile dump complete")
                time.sleep(2)  # let the last bytes flush
                vbm("controlvm", name, "poweroff", check=False)
                time.sleep(2)
                print(f"COM1 log: {com1_log}")
                print("Next step: python3 scripts/extract_pgo_profile.py "
                      f"--log {com1_log} --out <out.profraw> --merge <out.profdata>")
                return

    print("WARNING: timed out waiting for XNPGO_END", file=sys.stderr)
    vbm("controlvm", name, "poweroff", check=False)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(
        description="VirtualBox VM lifecycle manager for XINIM x86_32 lanes")
    ap.add_argument("--lane", default="i486",
                    help="CPU lane (i486, i686, etc.)  (default: i486)")
    ap.add_argument("--build-dir", default="build/i486/Debug",
                    help="CMake build directory (default: build/i486/Debug)")
    ap.add_argument("--project-root", default=".",
                    help="Project root directory (default: .)")

    sub = ap.add_subparsers(dest="command", required=True)

    sub.add_parser("setup",       help="Create/register VM from qcow2 image")
    sub.add_parser("start",       help="Start VM headless")
    sub.add_parser("stop",        help="Power off VM")
    sub.add_parser("teardown",    help="Power off + unregister + delete VM")
    sub.add_parser("status",      help="Print VM power state")
    sub.add_parser("test",        help="Run test suite against running VM")
    sub.add_parser("pgo-collect", help="Boot instrumented kernel; wait for profile dump")

    ss = sub.add_parser("screenshot", help="Take a PNG screenshot")
    ss.add_argument("--name", default="snapshot", help="Screenshot base name")

    args = ap.parse_args()

    dispatch = {
        "setup":       cmd_setup,
        "start":       cmd_start,
        "stop":        cmd_stop,
        "teardown":    cmd_teardown,
        "status":      cmd_status,
        "test":        cmd_test,
        "screenshot":  cmd_screenshot,
        "pgo-collect": cmd_pgo_collect,
    }
    dispatch[args.command](args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
