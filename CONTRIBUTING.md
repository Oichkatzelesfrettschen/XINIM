# Contributing

This project employs [pre-commit](https://pre-commit.com/) to consolidate
formatting and static-analysis tasks. Each hook calls a wrapper under
`tools/` so local runs mirror continuous integration.

## Local Setup

```bash
sudo apt-get update
sudo apt-get install clang-format clang-tidy cppcheck cloc cscope
pip install pre-commit lizard
pre-commit install
```

## Running Checks

Execute the full suite before pushing changes:

```bash
pre-commit run --all-files
```

The invocation above sequentially runs:

- `tools/pre-commit-clang-format.sh` for style enforcement
- `tools/run_clang_tidy.sh` for semantic analysis
- `tools/run_cppcheck.sh` for static diagnostics and code metrics

GitHub Actions repeats the same command for every push and pull request
via `.github/workflows/pre-commit.yml`.
