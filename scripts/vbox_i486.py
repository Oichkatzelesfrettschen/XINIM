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


VM_NAME = "XINIM-i486"
BOOT_WAIT_S = 20          # seconds to wait after startvm before issuing commands
PGO_TIMEOUT_S = 300       # maximum seconds to wait for XNPGO_END after boot


def vbm(*args: str, check: bool = True, capture: bool = False) -> str:
    """Run a VBoxManage command.  Returns stdout when capture=True."""
    cmd = ["VBoxManage"] + list(args)
    if capture:
        r = subprocess.run(cmd, check=check, capture_output=True, text=True)
        return r.stdout
    subprocess.run(cmd, check=check)
    return ""


def vm_exists() -> bool:
    try:
        out = vbm("showvminfo", VM_NAME, "--machinereadable",
                  check=False, capture=True)
        return "VMState=" in out
    except Exception:
        return False


def vm_running() -> bool:
    try:
        out = vbm("showvminfo", VM_NAME, "--machinereadable",
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

    imgdir = Path(args.build_dir) / "images" / "i486"
    logdir = Path(args.build_dir) / "logs"
    qcow2 = imgdir / "xinim-i486-boot.qcow2"
    vdi = imgdir / f"{VM_NAME}.vdi"
    com1_log = logdir / "vbox-com1.log"

    if not qcow2.is_file():
        sys.exit(f"error: boot image not found: {qcow2}\n"
                 "       Build the 'xinim_i486_image' CMake target first.")

    logdir.mkdir(parents=True, exist_ok=True)

    # Convert qcow2 -> raw -> VDI
    raw = imgdir / f"{VM_NAME}-tmp.raw"
    print(f"Converting {qcow2.name} -> VDI ...")
    subprocess.run(["qemu-img", "convert", "-f", "qcow2", "-O", "raw",
                    str(qcow2), str(raw)], check=True)
    if vdi.exists():
        vdi.unlink()
    vbm("convertfromraw", str(raw), str(vdi), "--format", "VDI")
    raw.unlink(missing_ok=True)
    print(f"VDI: {vdi}")

    # Tear down stale VM if it exists
    if vm_exists():
        if vm_running():
            vbm("controlvm", VM_NAME, "poweroff", check=False)
            time.sleep(2)
        vbm("unregistervm", VM_NAME, "--delete", check=False)
        time.sleep(1)

    # Create VM
    vbm("createvm", "--name", VM_NAME, "--ostype", "Linux",
        "--register", "--basefolder", str(logdir))
    vbm("modifyvm", VM_NAME,
        "--cpus", "1",
        "--memory", "64",
        "--vram", "16",
        "--firmware", "bios",
        "--graphicscontroller", "vmsvga",
        "--audio-driver", "none",
        "--usb", "off",
        "--ioapic", "off",
        "--pae", "off",
        "--uart1", "0x3F8", "4",
        "--uart-mode1", "file", str(com1_log),
        "--nic1", "none")
    vbm("storagectl", VM_NAME, "--name", "IDE", "--add", "ide",
        "--controller", "PIIX4")
    vbm("storageattach", VM_NAME, "--storagectl", "IDE",
        "--port", "0", "--device", "0", "--type", "hdd", "--medium", str(vdi))
    print(f"VM '{VM_NAME}' registered.  COM1 -> {com1_log}")


def cmd_start(args: argparse.Namespace) -> None:
    require_vboxmanage()
    if not vm_exists():
        sys.exit(f"error: VM '{VM_NAME}' not found -- run 'setup' first")
    if vm_running():
        print(f"VM '{VM_NAME}' is already running")
        return
    vbm("startvm", VM_NAME, "--type", "headless")
    print(f"VM '{VM_NAME}' started headless.  Waiting {BOOT_WAIT_S}s for boot ...")
    time.sleep(BOOT_WAIT_S)


def cmd_stop(args: argparse.Namespace) -> None:
    require_vboxmanage()
    if not vm_running():
        print(f"VM '{VM_NAME}' is not running")
        return
    vbm("controlvm", VM_NAME, "poweroff", check=False)
    time.sleep(2)
    print(f"VM '{VM_NAME}' stopped")


def cmd_teardown(args: argparse.Namespace) -> None:
    require_vboxmanage()
    if vm_running():
        vbm("controlvm", VM_NAME, "poweroff", check=False)
        time.sleep(2)
    if vm_exists():
        vbm("unregistervm", VM_NAME, "--delete", check=False)
        print(f"VM '{VM_NAME}' removed")
    else:
        print(f"VM '{VM_NAME}' does not exist")


def cmd_screenshot(args: argparse.Namespace) -> None:
    require_vboxmanage()
    logdir = Path(args.build_dir) / "logs"
    outdir = logdir / "vbox-screenshots"
    outdir.mkdir(parents=True, exist_ok=True)
    name = args.name or "snapshot"
    png = outdir / f"{name}.png"
    vbm("controlvm", VM_NAME, "screenshotpng", str(png))
    print(f"Screenshot: {png}")


def cmd_status(args: argparse.Namespace) -> None:
    require_vboxmanage()
    if not vm_exists():
        print(f"VM '{VM_NAME}': not registered")
        return
    out = vbm("showvminfo", VM_NAME, "--machinereadable", capture=True)
    for line in out.splitlines():
        if line.startswith("VMState="):
            print(f"VM '{VM_NAME}': {line}")
            return


def cmd_test(args: argparse.Namespace) -> None:
    """Run the existing bash test suite against the running VM."""
    require_vboxmanage()
    script = Path(args.project_root) / "scripts" / "test_i486_vbox.sh"
    if not script.is_file():
        sys.exit(f"error: test script not found: {script}")
    env = dict(os.environ)
    env["BUILD_DIR"] = str(args.build_dir)
    subprocess.run(["bash", str(script)], env=env, check=True)


def cmd_pgo_collect(args: argparse.Namespace) -> None:
    """Boot the PGO-instrumented kernel; wait for XNPGO_END in COM1 log."""
    require_vboxmanage()
    logdir = Path(args.build_dir) / "logs"
    com1_log = logdir / "vbox-pgo-com1.log"

    if not vm_exists():
        sys.exit(f"error: VM '{VM_NAME}' not found -- run 'setup' first with "
                 "the PGO-instrumented disk image attached")

    # Clear old log
    if com1_log.exists():
        com1_log.unlink()
    # Update COM1 log path to PGO-specific file
    if vm_running():
        vbm("controlvm", VM_NAME, "poweroff", check=False)
        time.sleep(2)
    vbm("modifyvm", VM_NAME,
        "--uart-mode1", "file", str(com1_log))

    print("Starting VM for PGO profile collection ...")
    vbm("startvm", VM_NAME, "--type", "headless")

    deadline = time.monotonic() + PGO_TIMEOUT_S
    print(f"Waiting up to {PGO_TIMEOUT_S}s for XNPGO_END in {com1_log} ...")
    while time.monotonic() < deadline:
        time.sleep(5)
        if com1_log.is_file():
            content = com1_log.read_text(errors="replace")
            if "XNPGO_END" in content:
                print("XNPGO_END detected -- profile dump complete")
                time.sleep(2)  # let the last bytes flush
                vbm("controlvm", VM_NAME, "poweroff", check=False)
                time.sleep(2)
                print(f"COM1 log: {com1_log}")
                print("Next step: python3 scripts/extract_pgo_profile.py "
                      f"--log {com1_log} --out <out.profraw> --merge <out.profdata>")
                return

    print("WARNING: timed out waiting for XNPGO_END", file=sys.stderr)
    vbm("controlvm", VM_NAME, "poweroff", check=False)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(
        description="VirtualBox VM lifecycle manager for XINIM i486")
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
