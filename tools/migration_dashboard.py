#!/usr/bin/env python3
"""Generate modernization progress report."""

import json
from pathlib import Path
import networkx as nx

SUMMARY_FILE = Path('kr_cpp_summary.json')

CATEGORY_PREFIXES = ['fs/', 'kernel/', 'mm/', 'lib/', 'commands/', 'tools/', 'test/']


def analyze_dependencies(files):
    deps = {f: [] for f in files}
    for file in files:
        text = Path(file).read_text(errors='ignore')
        for other in files:
            if other != file and Path(other).name in text:
                deps[file].append(other)
    return deps


def prioritize(files, deps):
    G = nx.DiGraph()
    for f, ds in deps.items():
        for d in ds:
            G.add_edge(d, f)
    try:
        return list(nx.topological_sort(G))
    except nx.NetworkXUnfeasible:
        return sorted(files, key=lambda x: G.in_degree(x))


def generate_report():
    data = json.loads(SUMMARY_FILE.read_text())
    files = data['files']
    categories = {p: [] for p in CATEGORY_PREFIXES}
    for f in files:
        for p in CATEGORY_PREFIXES:
            if f.startswith(p):
                categories[p].append(f)
                break
    print('MINIX1 K&R Modernization Priority List')
    for cat in CATEGORY_PREFIXES:
        subset = categories[cat]
        if not subset:
            continue
        deps = analyze_dependencies(subset)
        order = prioritize(subset, deps)
        print(f"\n{cat} ({len(subset)} files)")
        for i, f in enumerate(order[:5]):
            print(f"  {i+1}. {f}")
        if len(subset) > 5:
            print(f"  ... and {len(subset)-5} more")

if __name__ == '__main__':
    generate_report()
