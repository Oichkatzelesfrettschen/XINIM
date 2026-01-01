"""Generate AST-level dependency graphs across repository languages.

This utility leverages Tree-sitter grammars to extract import and include
relationships for multiple languages. The resulting graphs are emitted as JSON
structures that can be consumed by reporting pipelines or Sphinx documentation.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Mapping, MutableMapping, MutableSequence, Set

from tree_sitter_languages import get_language, get_parser

# Supported file extensions mapped to Tree-sitter language identifiers.
LANGUAGE_MAP: Mapping[str, str] = {
    ".c": "c",
    ".h": "c",
    ".cpp": "cpp",
    ".hpp": "cpp",
    ".cppm": "cpp",
    ".S": "c",  # Preprocessor-style includes are common.
    ".py": "python",
    ".sh": "bash",
    ".lua": "lua",
    ".pl": "perl",
}

# Directories that should be skipped to avoid generated artifacts and vendored trees.
DEFAULT_OMIT: Set[str] = {".git", "build", "_build", "third_party"}


@dataclass(slots=True)
class DependencyGraph:
    """In-memory directed dependency graph keyed by language and file path."""

    edges: MutableMapping[str, Set[str]] = field(default_factory=lambda: defaultdict(set))

    def add(self, source: str, targets: Iterable[str]) -> None:
        """Record dependencies from ``source`` to each entry in ``targets``."""

        if not targets:
            return
        for target in targets:
            if target:
                self.edges[source].add(target)

    def to_serializable(self) -> Mapping[str, list[str]]:
        """Return a JSON-friendly representation of the graph."""

        return {key: sorted(values) for key, values in self.edges.items()}

    def edge_count(self) -> int:
        """Return the number of directed edges in the graph."""

        return sum(len(targets) for targets in self.edges.values())


def _strip_quotes(raw: str) -> str:
    """Remove surrounding quotes or angle brackets from preprocessor paths or import strings."""

    if len(raw) >= 2 and raw[0] in {'"', "'", '<'} and raw[-1] in {raw[0], '>'}:
        if (raw[0] == '<' and raw[-1] == '>') or (raw[0] == raw[-1]):
            return raw[1:-1]
    return raw


def _query_c_includes(source_bytes: bytes, language_id: str) -> list[str]:
    language = get_language(language_id)
    parser = get_parser(language_id)
    tree = parser.parse(source_bytes)
    query = language.query("(preproc_include path: [(string_literal) (system_lib_string)] @include)")
    includes: list[str] = []
    for node, capture in query.captures(tree.root_node):
        if capture == "include":
            text = source_bytes[node.start_byte : node.end_byte].decode()
            includes.append(_strip_quotes(text))
    if not includes:
        includes.extend(_regex_c_includes(source_bytes.decode(errors="ignore")))
    return includes


def _query_python_imports(source_bytes: bytes, language_id: str) -> list[str]:
    language = get_language(language_id)
    parser = get_parser(language_id)
    tree = parser.parse(source_bytes)
    modules: list[str] = []
    try:
        query = language.query(
            "(import_statement (dotted_name) @module)"
            "\n(import_from_statement module_name: (dotted_name) @module)"
        )
        for node, capture in query.captures(tree.root_node):
            if capture == "module":
                modules.append(source_bytes[node.start_byte : node.end_byte].decode())
    except Exception:
        modules.extend(_regex_python_imports(source_bytes.decode(errors="ignore")))
    if not modules:
        modules.extend(_regex_python_imports(source_bytes.decode(errors="ignore")))
    return modules


def _query_bash_sources(source_bytes: bytes, language_id: str) -> list[str]:
    language = get_language(language_id)
    parser = get_parser(language_id)
    tree = parser.parse(source_bytes)
    paths: list[str] = []
    try:
        query = language.query(
            "((command name: (command_name) @cmd argument: (word) @path)"
            " (#match? @cmd \"source|\\.\"))"
        )
        for node, capture in query.captures(tree.root_node):
            if capture == "path":
                paths.append(source_bytes[node.start_byte : node.end_byte].decode())
    except Exception:
        paths.extend(_regex_bash_sources(source_bytes.decode(errors="ignore")))
    if not paths:
        paths.extend(_regex_bash_sources(source_bytes.decode(errors="ignore")))
    return paths


def _query_lua_requires(source_bytes: bytes, language_id: str) -> list[str]:
    language = get_language(language_id)
    parser = get_parser(language_id)
    tree = parser.parse(source_bytes)
    modules: list[str] = []
    try:
        query = language.query(
            "((function_call name: (identifier) @func arguments: (arguments (string) @mod))"
            " (#eq? @func \"require\"))"
        )
        for node, capture in query.captures(tree.root_node):
            if capture == "mod":
                modules.append(_strip_quotes(source_bytes[node.start_byte : node.end_byte].decode()))
    except Exception:
        modules.extend(_regex_lua_requires(source_bytes.decode(errors="ignore")))
    if not modules:
        modules.extend(_regex_lua_requires(source_bytes.decode(errors="ignore")))
    return modules


def _query_perl_uses(source_bytes: bytes, language_id: str) -> list[str]:
    language = get_language(language_id)
    parser = get_parser(language_id)
    tree = parser.parse(source_bytes)
    modules: list[str] = []
    try:
        query = language.query("((use_statement module: (scoped_identifier) @module))")
        for node, capture in query.captures(tree.root_node):
            if capture == "module":
                modules.append(source_bytes[node.start_byte : node.end_byte].decode())
    except Exception:
        modules.extend(_regex_perl_uses(source_bytes.decode(errors="ignore")))
    if not modules:
        modules.extend(_regex_perl_uses(source_bytes.decode(errors="ignore")))
    return modules


def _regex_asm_includes(source_text: str) -> list[str]:
    pattern = re.compile(r"^\s*(?:%include|\.include)\s+([\"']?)([^\s\"']+)\1", re.MULTILINE)
    return [match.group(2) for match in pattern.finditer(source_text)]


def _regex_c_includes(source_text: str) -> list[str]:
    pattern = re.compile(r"^\s*#\s*include\s+[<\"]([^>\"]+)[>\"]", re.MULTILINE)
    return pattern.findall(source_text)


def _regex_lua_requires(source_text: str) -> list[str]:
    pattern = re.compile(r"require\s*\(\s*['\"]([^'\"]+)['\"]\s*\)")
    return pattern.findall(source_text)


def _regex_perl_uses(source_text: str) -> list[str]:
    pattern = re.compile(r"^\s*use\s+([\w:]+)", re.MULTILINE)
    return pattern.findall(source_text)


def _regex_python_imports(source_text: str) -> list[str]:
    pattern = re.compile(r"^\s*(?:from\s+([\w\.]+)\s+import|import\s+([\w\.]+))", re.MULTILINE)
    imports: list[str] = []
    for match in pattern.finditer(source_text):
        module = match.group(1) or match.group(2)
        if module:
            imports.append(module)
    return imports


def _regex_bash_sources(source_text: str) -> list[str]:
    pattern = re.compile(r"^\s*(?:source|\.)\s+([^\s]+)", re.MULTILINE)
    return pattern.findall(source_text)


QUERY_DISPATCH = {
    "c": _query_c_includes,
    "cpp": _query_c_includes,
    "python": _query_python_imports,
    "bash": _query_bash_sources,
    "lua": _query_lua_requires,
    "perl": _query_perl_uses,
}


def detect_language(path: Path) -> str | None:
    """Return the Tree-sitter language id for ``path`` if supported."""

    return LANGUAGE_MAP.get(path.suffix)


def analyze_file(path: Path) -> tuple[str | None, list[str]]:
    """Analyze a single file and return its language id and dependencies."""

    language_id = detect_language(path)
    if language_id is None:
        return None, []

    source_bytes = path.read_bytes()
    if language_id in QUERY_DISPATCH:
        dependencies = QUERY_DISPATCH[language_id](source_bytes, language_id)
    elif path.suffix == ".S":
        dependencies = _regex_asm_includes(source_bytes.decode(errors="ignore"))
    else:
        dependencies = []
    return language_id, dependencies


def iter_repository_files(root: Path, omit: Set[str]) -> Iterable[Path]:
    """Yield all regular files under ``root`` while skipping omitted directories."""

    for entry in root.rglob("*"):
        if not entry.is_file():
            continue
        if any(part in omit for part in entry.relative_to(root).parts):
            continue
        yield entry


def build_dependency_graph(root: Path, omit: Set[str]) -> tuple[Mapping[str, DependencyGraph], Counter]:
    """Build per-language dependency graphs and return them with file counts."""

    graphs: MutableMapping[str, DependencyGraph] = defaultdict(DependencyGraph)
    language_counts: Counter[str] = Counter()

    for path in iter_repository_files(root, omit):
        language_id, dependencies = analyze_file(path)
        if language_id is None:
            continue
        rel_path = str(path.relative_to(root))
        graphs[language_id].add(rel_path, dependencies)
        language_counts[language_id] += 1
    return graphs, language_counts


def summarize_graphs(graphs: Mapping[str, DependencyGraph], counts: Counter) -> dict:
    """Generate an aggregate summary suitable for reporting."""

    summary = {
        "languages": {},
        "totals": {
            "languages": len(graphs),
            "files": sum(counts.values()),
            "edges": sum(graph.edge_count() for graph in graphs.values()),
        },
    }
    for language, graph in graphs.items():
        degree_counter = {source: len(targets) for source, targets in graph.edges.items()}
        heavy_hitters = sorted(degree_counter.items(), key=lambda item: item[1], reverse=True)[:10]
        summary["languages"][language] = {
            "files": counts.get(language, 0),
            "edges": graph.edge_count(),
            "top_emitters": heavy_hitters,
        }
    return summary


def main(argv: MutableSequence[str] | None = None) -> None:
    """Parse arguments and write dependency graph artifacts."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."), help="Repository root to scan.")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("docs/analysis/ast_dependency_graphs.json"),
        help="Path for the detailed graph JSON output.",
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("docs/analysis/ast_dependency_summary.json"),
        help="Path for the summary statistics JSON output.",
    )
    parser.add_argument(
        "--omit",
        action="append",
        default=list(DEFAULT_OMIT),
        help="Directories to omit while scanning.",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    omit = set(args.omit)
    graphs, counts = build_dependency_graph(root, omit)

    output_payload = {lang: graph.to_serializable() for lang, graph in graphs.items()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output_payload, indent=2, sort_keys=True))

    summary_payload = summarize_graphs(graphs, counts)
    args.summary.write_text(json.dumps(summary_payload, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
