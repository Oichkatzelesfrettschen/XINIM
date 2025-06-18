#!/usr/bin/env python3
"""Automated K&R to C++23 modernization tool."""

import argparse
from pathlib import Path
import re

KR_PATTERN = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*\s+)?([A-Za-z_][A-Za-z0-9_]*)\(([^)])*\)\s*\n([^{]+);", re.MULTILINE)

def modernize_file(path: Path, dry_run: bool, no_backup: bool) -> None:
    content = path.read_text()
    matches = list(KR_PATTERN.finditer(content))
    new_content = content
    for m in reversed(matches):
        ret = m.group(1).strip() if m.group(1) else 'int'
        name = m.group(2)
        params = m.group(3).strip()
        decls = m.group(4).strip().split('\n')
        param_map = {}
        for decl in decls:
            decl = decl.rstrip(';').strip()
            if not decl:
                continue
            t, var = decl.rsplit(' ', 1)
            param_map[var] = t
        param_list = []
        if params:
            for p in params.split(','):
                p = p.strip()
                t = param_map.get(p, 'int')
                param_list.append(f"{t} {p}")
        sig = f"{ret} {name}({', '.join(param_list) if param_list else 'void'})"
        new_content = new_content[:m.start()] + sig + new_content[m.end():]
    if new_content != content:
        if not dry_run and not no_backup:
            path.with_suffix('.kr_backup').write_text(content)
        if not dry_run:
            path.write_text(new_content)
        print(f"Modernized {path}")
    else:
        print(f"No K&R style found in {path}")

def main():
    parser = argparse.ArgumentParser(description="modernize K&R C++")
    parser.add_argument('files', nargs='+')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--no-backup', action='store_true')
    args = parser.parse_args()
    for f in args.files:
        modernize_file(Path(f), args.dry_run, args.no_backup)

if __name__ == '__main__':
    main()
