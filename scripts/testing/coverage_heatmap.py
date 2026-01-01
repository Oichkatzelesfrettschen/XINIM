"""Generate a lightweight HTML coverage heatmap from LLVM coverage exports."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, Sequence


def _require_tool(name: str) -> Path:
    path = shutil.which(name)
    if not path:
        raise SystemExit(f"Required tool '{name}' not found on PATH")
    return Path(path)


def export_coverage(llvm_cov: Path, binary: Path, profdata: Path, source_root: Path) -> Dict[str, float]:
    """Runs llvm-cov export and returns a mapping of file -> coverage percent."""

    export_cmd = [
        str(llvm_cov),
        "export",
        f"-instr-profile={profdata}",
        str(binary),
        "-format=json",
        f"-object={binary}",
    ]
    process = subprocess.run(export_cmd, check=True, capture_output=True, text=True)
    payload = json.loads(process.stdout)

    result: Dict[str, float] = {}
    for file_report in payload.get("data", []):
        for file_entry in file_report.get("files", []):
            filename = Path(file_entry["filename"]).resolve()
            if source_root not in filename.parents:
                continue
            segments = file_entry.get("segments", [])
            covered = sum(1 for seg in segments if seg[2] > 0)
            total = len(segments)
            result[str(filename.relative_to(source_root))] = (covered / total) * 100 if total else 0.0
    return result


def render_heatmap(coverage: Dict[str, float]) -> str:
    """Creates a single-file HTML heatmap."""

    rows = "\n".join(
        f"<tr><td>{fname}</td><td><div class='bar' style='width:{pct:.1f}%;'></div></td><td>{pct:.1f}%</td></tr>"
        for fname, pct in sorted(coverage.items())
    )
    return f"""
<!doctype html>
<html lang=\"en\">
<head>
<meta charset=\"utf-8\" />
<title>XINIM Coverage Heatmap</title>
<style>
body {{ font-family: system-ui, sans-serif; margin: 2rem; }}
table {{ width: 100%; border-collapse: collapse; }}
th, td {{ padding: 0.5rem; border-bottom: 1px solid #ddd; }}
.bar {{ height: 1rem; background: linear-gradient(90deg, #c0392b, #27ae60); }}
</style>
</head>
<body>
<h1>Coverage Heatmap</h1>
<table>
<thead><tr><th>File</th><th>Coverage</th><th>%</th></tr></thead>
<tbody>
{rows}
</tbody>
</table>
</body>
</html>
"""


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path, help="Instrumented binary to analyze")
    parser.add_argument("--profdata", required=True, type=Path, help="llvm-profdata merged profile")
    parser.add_argument("--source-root", type=Path, default=Path.cwd(), help="Root of the source tree")
    parser.add_argument("--output", type=Path, default=Path("coverage-heatmap.html"), help="Output HTML path")
    parser.add_argument("--llvm-cov", type=str, default="llvm-cov", help="llvm-cov executable")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    llvm_cov = _require_tool(args.llvm_cov)
    coverage = export_coverage(llvm_cov, args.binary, args.profdata, args.source_root.resolve())
    args.output.write_text(render_heatmap(coverage), encoding="utf-8")
    print(f"wrote coverage heatmap to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
