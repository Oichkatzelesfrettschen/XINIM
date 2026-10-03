# Analysis pylint

```text
************* Module 2.py
2.py:1:0: F0001: No module named 2.py (fatal)
2.py:1:0: F0001: No module named 2.py (fatal)
************* Module conf
docs/sphinx/conf.py:16:0: C0103: Constant name "project" doesn't conform to UPPER_CASE naming style (invalid-name)
docs/sphinx/conf.py:20:0: C0103: Constant name "author" doesn't conform to UPPER_CASE naming style (invalid-name)
docs/sphinx/conf.py:21:0: C0103: Constant name "version" doesn't conform to UPPER_CASE naming style (invalid-name)
docs/sphinx/conf.py:22:0: C0103: Constant name "release" doesn't conform to UPPER_CASE naming style (invalid-name)
docs/sphinx/conf.py:41:0: C0103: Constant name "breathe_default_project" doesn't conform to UPPER_CASE naming style (invalid-name)
docs/sphinx/conf.py:44:0: C0103: Constant name "html_theme" doesn't conform to UPPER_CASE naming style (invalid-name)
************* Module arch_scan
tools/arch_scan.py:65:13: W1514: Using open without explicitly specifying an encoding (unspecified-encoding)
************* Module ascii_tree
tools/ascii_tree.py:1:0: C0114: Missing module docstring (missing-module-docstring)
************* Module tools/classify_style
tools/classify_style.py:1:0: C0103: Module name "tools/classify_style" doesn't conform to snake_case naming style (invalid-name)
tools/classify_style.py:36:13: W1514: Using open without explicitly specifying an encoding (unspecified-encoding)
tools/classify_style.py:6:0: W0611: Unused import re (unused-import)
************* Module find_knr
tools/find_knr.py:12:0: C0413: Import "from classify_style import classify_file" should be placed at the top of the module (wrong-import-position)
```

