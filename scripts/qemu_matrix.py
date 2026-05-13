#!/usr/bin/env python3
"""Lane-aware QEMU matrix runner for XINIM."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
from typing import Iterable


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_BUILD_ROOT = REPO_ROOT / "build"


def default_build_dir() -> pathlib.Path:
    if "XINIM_ACTIVE_BUILD_DIR" in os.environ:
        return pathlib.Path(os.environ["XINIM_ACTIVE_BUILD_DIR"])
    if "XINIM_BUILD_ROOT" in os.environ:
        return pathlib.Path(os.environ["XINIM_BUILD_ROOT"])
    return DEFAULT_BUILD_ROOT / "x86_64" / "Debug"


def default_manifest_path(build_dir: pathlib.Path) -> pathlib.Path:
    return build_dir / "x86_cpu_lanes.json"


def load_manifest(path: pathlib.Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)
    except FileNotFoundError as exc:
        raise SystemExit(
            f"lane manifest not found at {path}. Configure the build tree first."
        ) from exc
    lanes = data.get("lanes")
    if not isinstance(lanes, dict):
        raise SystemExit(f"invalid lane manifest: {path}")
    return lanes


def run_command(command: list[str], dry_run: bool) -> int:
    print("$", " ".join(command), flush=True)
    if dry_run:
        return 0
    return subprocess.run(command, check=False).returncode


def lane_names_from_args(args: argparse.Namespace, manifest: dict) -> list[str]:
    if args.all_x86_32:
        return [name for name, meta in manifest.items() if meta.get("family") == "x86_32"]
    if args.lane:
        return args.lane
    raise SystemExit("specify --lane <name> or --all-x86-32")


def print_lane_list(manifest: dict) -> None:
    for lane in sorted(manifest):
        meta = manifest[lane]
        print(
            f"{lane:16} family={meta['family']:7} "
            f"cpu={meta['qemu_cpu']:10} image_target={meta['image_target']}"
        )


def prepare_lane(build_dir: pathlib.Path, lane_meta: dict, dry_run: bool) -> int:
    return run_command(
        ["cmake", "--build", str(build_dir), "--target", lane_meta["image_target"]],
        dry_run=dry_run,
    )


def ctest_for_name(build_dir: pathlib.Path, test_name: str, dry_run: bool) -> int:
    return run_command(
        [
            "ctest",
            "--output-on-failure",
            "--test-dir",
            str(build_dir),
            "-R",
            f"^({test_name})$",
        ],
        dry_run=dry_run,
    )


def test_lane(build_dir: pathlib.Path, lane_meta: dict, test_kinds: Iterable[str], dry_run: bool) -> int:
    test_name_map = {
        "prepare": lane_meta.get("prepare_test", ""),
        "smoke": lane_meta.get("smoke_test", ""),
        "layout": lane_meta.get("layout_test", ""),
        "shell": lane_meta.get("shell_test", ""),
    }

    expanded: list[str] = []
    for kind in test_kinds:
        if kind == "all":
            expanded.extend(["prepare", "smoke", "layout", "shell"])
        else:
            expanded.append(kind)

    for kind in expanded:
        test_name = test_name_map.get(kind, "")
        if not test_name:
            continue
        rc = ctest_for_name(build_dir, test_name, dry_run=dry_run)
        if rc != 0:
            return rc
    return 0


def launch_lane(
    lane_meta: dict,
    cpu_override: str | None,
    memory_override: str | None,
    boot_mode: str,
    dry_run: bool,
) -> int:
    launcher = pathlib.Path(lane_meta["launcher_script"])
    cpu = cpu_override or lane_meta["qemu_cpu"]
    memory = memory_override or lane_meta["qemu_memory"]
    machine = lane_meta["qemu_machine"]

    command = [str(launcher), "--cpu", cpu, "--machine", machine]
    if boot_mode == "disk":
        command.extend(["--boot-disk", lane_meta["boot_disk_path"]])
    else:
        command.extend(["--boot-image", lane_meta["image_path"]])
        ata_disk = lane_meta.get("ata_disk_path", "")
        if ata_disk:
            command.extend(["--disk-image", ata_disk])
    if lane_meta["family"] == "x86_32":
        command.extend(["--memory", memory, "--vga", lane_meta["qemu_vga"]])
    else:
        command.extend(["--memory", memory])
    return run_command(command, dry_run=dry_run)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default=str(default_build_dir()))
    parser.add_argument("--manifest")
    parser.add_argument("--lane", action="append", help="lane name to prepare, test, or launch")
    parser.add_argument("--all-x86-32", action="store_true", help="run across all registered 32-bit lanes")
    parser.add_argument("--list-lanes", action="store_true", help="print registered lanes and exit")
    parser.add_argument("--prepare", action="store_true", help="build the lane image target before running")
    parser.add_argument(
        "--test",
        action="append",
        choices=("prepare", "smoke", "layout", "shell", "all"),
        help="run ctest for the selected kind; may be repeated",
    )
    parser.add_argument("--launch", action="store_true", help="launch QEMU for the selected lane")
    parser.add_argument(
        "--boot-mode",
        choices=("disk", "iso"),
        default="disk",
        help="boot from the lane boot disk or ISO when launching (default: disk)",
    )
    parser.add_argument("--cpu-override", help="override the lane's default QEMU CPU model")
    parser.add_argument("--memory-override", help="override the lane's default guest RAM size")
    parser.add_argument("--dry-run", action="store_true", help="print commands without executing them")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    build_dir = pathlib.Path(args.build_dir)
    manifest_path = pathlib.Path(args.manifest) if args.manifest else default_manifest_path(build_dir)
    manifest = load_manifest(manifest_path)

    if args.list_lanes:
        print_lane_list(manifest)
        return 0

    lanes = lane_names_from_args(args, manifest)
    if args.launch and len(lanes) > 1:
        raise SystemExit("--launch only supports one lane at a time")

    for lane in lanes:
        if lane not in manifest:
            raise SystemExit(f"unknown lane: {lane}")
        lane_meta = manifest[lane]
        print(f"== {lane} ==", flush=True)
        if args.prepare:
            rc = prepare_lane(build_dir, lane_meta, dry_run=args.dry_run)
            if rc != 0:
                return rc
        if args.test:
            rc = test_lane(build_dir, lane_meta, args.test, dry_run=args.dry_run)
            if rc != 0:
                return rc
        if args.launch:
            rc = launch_lane(
                lane_meta,
                cpu_override=args.cpu_override,
                memory_override=args.memory_override,
                boot_mode=args.boot_mode,
                dry_run=args.dry_run,
            )
            if rc != 0:
                return rc
    return 0


if __name__ == "__main__":
    sys.exit(main())
