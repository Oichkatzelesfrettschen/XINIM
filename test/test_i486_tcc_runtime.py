"""Verify TinyCC search paths and dietlibc ownership through both disk stagers."""

import argparse
import importlib.util
from pathlib import Path
import re
import tempfile


def configured_path(config: str, name: str) -> str:
    match = re.search(rf'^#define {name} "([^"]+)"$', config, re.MULTILINE)
    if match is None:
        raise RuntimeError(f"missing configured path: {name}")
    return match.group(1)


def verify(args: argparse.Namespace) -> None:
    runtime = args.runtime_dir.resolve(strict=True)
    if (runtime / "crt1.o").read_bytes() != args.start_o.read_bytes():
        raise RuntimeError("TinyCC startup differs from the configured dietlibc startup")
    if (runtime / "libc.a").read_bytes() != args.dietlibc_a.read_bytes():
        raise RuntimeError("TinyCC libc differs from the configured dietlibc archive")
    config = args.config_header.read_text()
    crt_paths = configured_path(config, "CONFIG_TCC_CRTPREFIX").split(":")
    library_paths = configured_path(config, "CONFIG_TCC_LIBPATHS").split(":")
    tcc_path = configured_path(config, "CONFIG_TCCDIR")
    scripts = Path(__file__).resolve().parents[1] / "scripts"

    for builder in ("create_i486_ata_disk", "create_i486_boot_disk"):
        spec = importlib.util.spec_from_file_location(builder, scripts / f"{builder}.py")
        if spec is None or spec.loader is None:
            raise RuntimeError(f"cannot load disk stager: {builder}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix="xinim-tcc-staging-") as directory:
            root = Path(directory)
            module.install_tcc_runtime(str(root), str(runtime))
            for name in ("crt1.o", "crti.o", "crtn.o"):
                candidates = [root / path.lstrip("/") / name for path in crt_paths]
                if not any(candidate.is_file() and candidate.read_bytes() ==
                           (runtime / name).read_bytes() for candidate in candidates):
                    raise RuntimeError(f"{builder}: configured CRT search cannot find {name}")
            candidates = [root / path.lstrip("/") / "libc.a" for path in library_paths]
            if not any(candidate.is_file() and candidate.read_bytes() ==
                       args.dietlibc_a.read_bytes() for candidate in candidates):
                raise RuntimeError(f"{builder}: configured library search cannot find dietlibc")
            staged_support = root / tcc_path.lstrip("/") / "libtcc1.a"
            if staged_support.read_bytes() != (runtime / "tcc" / "libtcc1.a").read_bytes():
                raise RuntimeError(f"{builder}: TinyCC support archive differs")
    print("TinyCC CRT search, dietlibc ownership, and both disk staging checks passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-dir", type=Path, required=True)
    parser.add_argument("--config-header", type=Path, required=True)
    parser.add_argument("--start-o", type=Path, required=True)
    parser.add_argument("--dietlibc-a", type=Path, required=True)
    verify(parser.parse_args())


if __name__ == "__main__":
    main()
