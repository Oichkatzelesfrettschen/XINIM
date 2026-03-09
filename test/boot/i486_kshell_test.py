#!/usr/bin/env python3
"""Compatibility wrapper for the generic 32-bit shell test."""

import os
import sys


LANE_NAME = os.environ.get("XINIM_BOOT_LANE_NAME", "i486")

CPU_BY_LANE = {
    "i486": "486",
    "i586": "pentium",
    "i686": "pentium3",
}
SHELL_PORT_BY_LANE = {
    "i486": "4556",
    "i586": "4557",
    "i686": "4558",
}

DEFAULT_IMAGE_NAME_BY_LANE = {
    "i486": "xinim-i486dx.iso",
    "i586": "xinim-i586.iso",
    "i686": "xinim-i686.iso",
}

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GENERIC_TEST = os.path.join(SCRIPT_DIR, "x86_32_shell_test.py")

os.environ.setdefault("XINIM_BOOT_LANE_NAME", LANE_NAME)
os.environ.setdefault("XINIM_BOOT_LANE_BANNER", LANE_NAME)
os.environ.setdefault("XINIM_QEMU_CPU", CPU_BY_LANE.get(LANE_NAME, "486"))
os.environ.setdefault("XINIM_QEMU_SHELL_PORT", SHELL_PORT_BY_LANE.get(LANE_NAME, "4556"))

if "XINIM_QEMU_BOOT_IMAGE" not in os.environ:
    project_root = os.environ.get("XINIM_PROJECT_ROOT", os.getcwd())
    state_root = os.environ.get("XINIM_STATE_ROOT", os.path.join(project_root, "build", "_state"))
    image_root = os.environ.get(
        "XINIM_IMAGE_ROOT",
        os.path.join(project_root, "build", LANE_NAME, "Debug", "images", LANE_NAME),
    )
    image_name = DEFAULT_IMAGE_NAME_BY_LANE.get(LANE_NAME, f"xinim-{LANE_NAME}.iso")
    candidate_roots = [
        os.path.join(project_root, "build", LANE_NAME, "Debug"),
        os.path.join(project_root, "build", f"{LANE_NAME}-cross", "Debug"),
        os.path.join(state_root, "build", "Debug"),
        state_root,
    ]
    for candidate_root in candidate_roots:
        image_root = os.path.join(candidate_root, "images")
        candidate = os.path.join(image_root, LANE_NAME, image_name)
        if os.path.exists(candidate):
            os.environ["XINIM_QEMU_BOOT_IMAGE"] = candidate
            break
    else:
        image_root = os.path.join(state_root, "images")
        image_name = DEFAULT_IMAGE_NAME_BY_LANE.get(LANE_NAME, f"xinim-{LANE_NAME}.iso")
        os.environ["XINIM_QEMU_BOOT_IMAGE"] = os.path.join(image_root, LANE_NAME, image_name)

os.environ.setdefault("XINIM_QEMU_SYSTEM_BIN", "qemu-system-i386")
os.environ.setdefault("XINIM_QEMU_MACHINE", "pc")
os.environ.setdefault("XINIM_QEMU_MEMORY", "32M")
os.environ.setdefault("XINIM_QEMU_VGA", "std")

os.execv(sys.executable, [sys.executable, GENERIC_TEST])
