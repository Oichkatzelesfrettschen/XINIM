#!/usr/bin/env python3
"""
Build cross-compiled package archive for XINIM i486 pkgsrc Track A.

Creates tar packages from cross-compiled tools that can be installed
on the target via pkg_add.

Usage:
    python3 scripts/build_i486_packages.py \
        --build-dir build/i486/Debug \
        --output-dir build/i486/Debug/packages
"""

import argparse
import os
import shutil
import tarfile
from pathlib import Path


def create_package(name: str, files: dict[str, str], output_dir: Path) -> Path:
    """
    Create a tar package.
    files: dict of {archive_path: source_path}
    """
    pkg_path = output_dir / f"{name}.tar"
    with tarfile.open(str(pkg_path), "w") as tf:
        for arcname, srcpath in sorted(files.items()):
            if os.path.exists(srcpath):
                tf.add(srcpath, arcname=arcname)
                print(f"  + {arcname}")
            else:
                print(f"  SKIP {arcname} (not found: {srcpath})")
    print(f"  -> {pkg_path}")
    return pkg_path


def build_dietlibc_dev_package(build_dir: Path, output_dir: Path) -> None:
    """Package dietlibc headers + library for on-target development."""
    tree_dir = build_dir / "dietlibc-i486" / "tree"
    if not tree_dir.exists():
        print(f"SKIP dietlibc-dev: {tree_dir} not found (build dietlibc first)")
        return

    files = {}
    include_dir = tree_dir / "include"
    if include_dir.exists():
        for root, _dirs, fnames in os.walk(str(include_dir)):
            for fname in fnames:
                src = os.path.join(root, fname)
                rel = os.path.relpath(src, str(tree_dir))
                files[f"usr/pkg/i486-dietlibc/{rel}"] = src

    lib_a = tree_dir / "bin-i386" / "dietlibc.a"
    if lib_a.exists():
        files["usr/pkg/i486-dietlibc/lib/dietlibc.a"] = str(lib_a)

    start_o = tree_dir / "bin-i386" / "start.o"
    if start_o.exists():
        files["usr/pkg/i486-dietlibc/lib/start.o"] = str(start_o)

    if files:
        print("Building dietlibc-dev package...")
        create_package("dietlibc-dev", files, output_dir)


def build_utility_package(name: str, binary_path: str, output_dir: Path) -> None:
    """Package a single utility binary."""
    if not os.path.exists(binary_path):
        print(f"SKIP {name}: {binary_path} not found")
        return
    files = {f"usr/pkg/bin/{name}": binary_path}
    print(f"Building {name} package...")
    create_package(name, files, output_dir)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"=== Building XINIM i486 package archive ===")
    print(f"Build dir: {build_dir}")
    print(f"Output dir: {output_dir}")

    # dietlibc development package (headers + .a)
    build_dietlibc_dev_package(build_dir, output_dir)

    # Cross-compiled tool packages
    tools = {
        "bmake": build_dir / "bmake-i486",
        "tcc": build_dir / "tcc-i486",
    }
    for name, path in tools.items():
        build_utility_package(name, str(path), output_dir)

    # List packages
    print("\n=== Package archive ===")
    for pkg in sorted(output_dir.glob("*.tar")):
        size_kb = pkg.stat().st_size // 1024
        print(f"  {pkg.name} ({size_kb} KB)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
