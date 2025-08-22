<!-- markdownlint-disable MD013 MD010 -->
# Tool Output Summary

This document captures sample runs of key analysis utilities on the repository. Each section is collapsible for easier navigation.

## Table of Contents

- [cloc](#cloc) - Lines of code analysis
- [sloccount](#sloccount) - Source lines of code counting
- [lizard](#lizard) - Cyclomatic complexity analysis
- [cscope](#cscope) - Source code cross-reference
- [diffoscope](#diffoscope) - File comparison and analysis
- [valgrind](#valgrind) - Memory debugging and profiling
- [cppcheck](#cppcheck) - Static code analysis for C/C++
- [flawfinder](#flawfinder) - Security vulnerability detection
- [gdb](#gdb) - GNU debugger information
- [pylint](#pylint) - Python code analysis
- [jscpd](#jscpd) - Copy-paste detection

---

<details>
<summary><h2 id="cloc">cloc</h2></summary>

```text
github.com/AlDanial/cloc v 1.98  T=0.46 s (1412.0 files/s, 300143.7 lines/s)
--------------------------------------------------------------------------------
Language                      files          blank        comment           code
--------------------------------------------------------------------------------
C++                             365           9139          13496          48640
Markdown                         58           1096              2          34974
C/C++ Header                    126           2253           3839           8776
XML                               1              0              0           4913
C                                13            369            960           2013
CMake                            18            243            336           1178
reStructuredText                 15            426            474           1156
YAML                              8             36             11            704
Text                              3            108              0            580
Bourne Shell                      9            156            152            564
Python                            8             87             67            366
JSON                              5              5              0            292
DOS Batch                        11              1              0            281
make                              5             50             82            155
Bourne Again Shell                2             14             13             59
Assembly                          1              8              7             26
Linker Script                     1              9              8             24
Dockerfile                        1              5              6              7
--------------------------------------------------------------------------------
SUM:                            650          14005          19453         104708
--------------------------------------------------------------------------------
```

</details>

<details>
<summary><h2 id="sloccount">sloccount</h2></summary>

```text
Have a non-directory at the top, so creating directory top_dir
Adding /workspace/XINIM/./CMakeLists.txt to top_dir
Adding /workspace/XINIM/./LICENSE to top_dir
Adding /workspace/XINIM/./project_status_2.md to top_dir
Adding /workspace/XINIM/./project_status_3.md to top_dir
Adding /workspace/XINIM/./project_status_4.md to top_dir
Adding /workspace/XINIM/./project_status.md to top_dir
Adding /workspace/XINIM/./README.md to top_dir
Adding /workspace/XINIM/./simd_library_complete_2.md to top_dir
Adding /workspace/XINIM/./simd_library_complete.md to top_dir
Creating filelist for build
Creating filelist for commands
Creating filelist for common
Creating filelist for crypto
Creating filelist for docs
Creating filelist for fs
Adding /workspace/XINIM/./grub_2.cfg to top_dir
Adding /workspace/XINIM/./grub.cfg to top_dir
Creating filelist for h
Creating filelist for hooks
Creating filelist for include
Creating filelist for kernel
Adding /workspace/XINIM/./kr_cpp_summary.json to top_dir
Creating filelist for lib
Adding /workspace/XINIM/./linker_2.ld to top_dir
Adding /workspace/XINIM/./linker.ld to top_dir
Adding /workspace/XINIM/./migrate_to_simd 2.sh to top_dir
Adding /workspace/XINIM/./migrate_to_simd.sh to top_dir
Creating filelist for mm
Adding /workspace/XINIM/./test_makefile to top_dir
Adding /workspace/XINIM/./test_makefile_2 to top_dir
Creating filelist for tests
Creating filelist for tools
Categorizing files.
Finding a working MD5 command....
Found a working MD5 command.
Warning: in tests, number of duplicates=65
Computing results.


SLOC    Directory       SLOC-by-Language (Sorted)
27121   commands        cpp=27121
7104    kernel          cpp=7078,asm=26
5102    tests           cpp=5093,ansic=9
4932    docs            xml=4913,python=19
4397    include         cpp=4387,ansic=10
4095    tools           cpp=3492,python=343,sh=260
3812    lib             cpp=3812
2844    fs              cpp=2844
1891    crypto          ansic=1671,cpp=220
1329    build           ansic=670,cpp=659
1320    mm              cpp=1320
573     common          cpp=573
467     h               cpp=467
301     top_dir         sh=301
10      hooks           sh=10


Totals grouped by language (dominant language first):
cpp:          57066 (87.39%)
xml:           4913 (7.52%)
ansic:         2360 (3.61%)
sh:             571 (0.87%)
python:         362 (0.55%)
asm:             26 (0.04%)




Total Physical Source Lines of Code (SLOC)                = 65,298
Development Effort Estimate, Person-Years (Person-Months) = 16.09 (193.13)
 (Basic COCOMO model, Person-Months = 2.4 * (KSLOC**1.05))
Schedule Estimate, Years (Months)                         = 1.54 (18.47)
 (Basic COCOMO model, Months = 2.5 * (person-months**0.38))
Estimated Average Number of Developers (Effort/Schedule)  = 10.45
Total Estimated Cost to Develop                           = $ 2,174,135
 (average salary = $56,286/year, overhead = 2.40).
SLOCCount, Copyright (C) 2001-2004 David A. Wheeler
SLOCCount is Open Source Software/Free Software, licensed under the GNU GPL.
SLOCCount comes with ABSOLUTELY NO WARRANTY, and you are welcome to
redistribute it under certain conditions as specified by the GNU GPL license;
see the documentation for details.
Please credit this data as "generated using David A. Wheeler's 'SLOCCount'."
```

</details>

<details>
<summary><h2 id="lizard">lizard</h2></summary>

```text
================================================
  NLOC    CCN   token  PARAM  length  location  
------------------------------------------------
      47     17    320      0      69 do_link@32-100@fs/link.cpp
      29      9    194      0      42 do_unlink@105-146@fs/link.cpp
      35      6    260      1      49 truncate@151-199@fs/link.cpp
      37      4    274      0      44 do_pipe@38-81@fs/pipe.cpp
      37     10    218      5      55 pipe_check@86-140@fs/pipe.cpp
      12      2     58      1      18 suspend@145-162@fs/pipe.cpp
      16      6    108      3      23 release@167-189@fs/pipe.cpp
      19      5     96      2      32 revive@194-225@fs/pipe.cpp
      32      9    225      0      39 do_unpause@230-268@fs/pipe.cpp
      22      6    181      0      34 do_chmod@37-70@fs/protect.cpp
      20      5    168      0      32 do_chown@75-106@fs/protect.cpp
       7      1     40      0      10 do_umask@112-121@fs/protect.cpp
      14      4    124      0      22 do_access@126-147@fs/protect.cpp
      36     11    238      3      58 forbidden@153-210@fs/protect.cpp
       9      2     57      1      14 read_only@216-229@fs/protect.cpp
      13      3    111      1      24 eat_path@30-53@fs/path.cpp
      23      6    169      1      40 last_dir@58-97@fs/path.cpp
```

</details>

<details>
<summary><h2 id="cscope">cscope</h2></summary>

```text
-rw-r--r-- 1 root root 4101706 Aug 12 19:41 cscope.out
```

</details>

<details>
<summary><h2 id="diffoscope">diffoscope</h2></summary>

```text
--- README.md
+++ docs/README_renamed.md
@@ -1,113 +1,352 @@
-# XINIM Project
-
-XINIM is a full C++23 reimplementation of the classic MINIX 1 operating system. The repository combines the kernel, memory management, file system, libraries, commands, and tooling into one cohesive environment. It demonstrates modern systems programming techniques with strong type safety, extensive error handling, and cross-platform support.
-
-For a comprehensive guide with build instructions and architectural details, refer to [docs/README_renamed.md](docs/README_renamed.md).
-
-**XINIM** is a clean-room C++ 23 re-implementation of the original MINIX 1 educational operating system.  
-The repository contains the full kernel, memory manager, file-system, classic user-land utilities, and supporting build scripts.
-
----
-
-## Table of Contents
-
-1. [Prerequisites](#prerequisites)  
-2. [Native Build (Quick Start)](#native-build-quick-start)  
-3. [Cross-Compilation](#cross-compilation)  
-4. [Cleaning the Workspace](#cleaning-the-workspace)  
```

</details>

<details>
<summary><h2 id="valgrind">valgrind</h2></summary>

```text
==8755== Memcheck, a memory error detector
==8755== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==8755== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==8755== Command: ls
==8755== 
CMakeLists.txt
LICENSE
project_status_2.md
project_status_3.md
project_status_4.md
project_status.md
README.md
simd_library_complete_2.md
simd_library_complete.md
build
commands
common
crypto
cscope.in.out
cscope.out
```

</details>

<details>
<summary><h2 id="cppcheck">cppcheck</h2></summary>

```text
fs/cache.cpp:1:1: error: The code contains unhandled character(s) (character code=226). Neither unicode nor extended ascii is supported. [syntaxError]
✅ All buffer cache primitives have now been elevated into a modern C++23 implementation, complete with RAII semantics (`BufferGuard`), strong `enum class` usage for block types, rigorous error handling, and clear semantic modularization.
^
fs/device.cpp:48:8: error: syntax error [syntaxError]
PUBLIC dev_close(dev)
       ^
fs/filedes.cpp:80:45: style: Parameter 'rip' can be declared as pointer to const [constParameterPointer]
PUBLIC struct filp *find_filp(struct inode *rip, int bits) {
                                            ^
fs/inode.cpp:223:14: style: C-style pointer casting [cstyleCast]
        copy((char *)rip, (char *)dip, INODE_SIZE); /* copy from blk to inode */
             ^
fs/inode.cpp:225:14: style: C-style pointer casting [cstyleCast]
        copy((char *)dip, (char *)rip, INODE_SIZE); /* copy from inode to blk */
             ^
fs/inode.cpp:109:9: style: The scope of the variable 'major' can be reduced. [variableScope]
    int major, minor; // For printf, int is fine
        ^
fs/inode.cpp:109:16: style: The scope of the variable 'minor' can be reduced. [variableScope]
    int major, minor; // For printf, int is fine
```

</details>

<details>
<summary><h2 id="flawfinder">flawfinder</h2></summary>

```text
Flawfinder version 2.0.19, (C) 2001-2019 David A. Wheeler.
Number of rules (primarily dangerous function names) in C/C++ ruleset: 222
Examining ./tests/test_service_manager_dag.cpp
Examining ./tests/test2 2.cpp
Examining ./tests/test_lattice_send_error 2.cpp
Examining ./tests/t15a.cpp
Examining ./tests/task_stubs.cpp
Examining ./tests/test7.cpp
Examining ./tests/test_lattice 2.cpp
Examining ./tests/test6 2.cpp
Examining ./tests/test_service_manager_updates 2.cpp
Examining ./tests/t11b 2.cpp
Examining ./tests/test4 2.cpp
Examining ./tests/test5 2.cpp
Examining ./tests/test_streams.cpp
Examining ./tests/test_lib.cpp
Examining ./tests/test_net_driver_ipv6 2.cpp
Examining ./tests/sodium_stub.cpp
Examining ./tests/test_net_driver_concurrency 2.cpp
Examining ./tests/t10a.cpp
```

</details>

<details>
<summary><h2 id="gdb">gdb</h2></summary>

```text
GNU gdb (Ubuntu 15.0.50.20240403-0ubuntu1) 15.0.50.20240403-git
Copyright (C) 2024 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
```

</details>

<details>
<summary><h2 id="pylint">pylint</h2></summary>

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

</details>

<details>
<summary><h2 id="flake8">flake8</h2></summary>

```text
2.py:0:1: E902 FileNotFoundError: [Errno 2] No such file or directory: '2.py'
2.py:0:1: E902 FileNotFoundError: [Errno 2] No such file or directory: '2.py'
docs/sphinx/conf.py:20:80: E501 line too long (81 > 79 characters)
tools/arch_scan.py:43:80: E501 line too long (80 > 79 characters)
tools/ascii_tree.py:81:80: E501 line too long (87 > 79 characters)
tools/classify_style:0:1: E902 FileNotFoundError: [Errno 2] No such file or directory: 'tools/classify_style'
tools/classify_style.py:6:1: F401 're' imported but unused
tools/classify_style.py:58:36: E203 whitespace before ':'
tools/classify_style.py:79:80: E501 line too long (82 > 79 characters)
tools/find_knr.py:12:1: E402 module level import not at top of file
tools/generate_kr_summary:0:1: E902 FileNotFoundError: [Errno 2] No such file or directory: 'tools/generate_kr_summary'
tools/migration_dashboard.py:10:80: E501 line too long (85 > 79 characters)
tools/migration_dashboard.py:56:1: E305 expected 2 blank lines after class or function definition, found 1
tools/modernize_kr_file.py:8:80: E501 line too long (119 > 79 characters)
tools/modernize_kr_file.py:10:1: E302 expected 2 blank lines, found 1
tools/modernize_kr_file.py:32:80: E501 line too long (80 > 79 characters)
tools/modernize_kr_file.py:43:1: E302 expected 2 blank lines, found 1
tools/modernize_kr_file.py:52:1: E305 expected 2 blank lines after class or function definition, found 1
```

</details>

<details>
<summary><h2 id="mypy">mypy</h2></summary>

```text
mypy: can't read file 'tools/classify_style': No such file or directory
```

</details>

<details>
<summary><h2 id="semgrep">semgrep</h2></summary>

```text
/root/.local/lib/python3.12/site-packages/opentelemetry/instrumentation/dependencies.py:4: UserWarning: pkg_resources is deprecated as an API. See https://setuptools.pypa.io/en/latest/pkg_resources.html. The pkg_resources package is slated for removal as early as 2025-11-30. Refrain from using this package or pin to Setuptools<81.
  from pkg_resources import (
                  
                  
┌────────────────┐
│ 1 Code Finding │
└────────────────┘
                
    tools/passwd
   ❯❯❱ generic.secrets.security.detected-etc-shadow.detected-etc-shadow
          linux shadow file detected  
          Details: https://sg.run/4ylL
                                      
            1┆ root::0:0::/:/bin/sh

```

</details>

<details>
<summary><h2 id="eslint">eslint</h2></summary>

```text
no js files
```

</details>

<details>
<summary><h2 id="jshint">jshint</h2></summary>

```text
no js files
```

</details>

<details>
<summary><h2 id="jscpd">jscpd</h2></summary>

```text
Clone found (cpp-header):
 - [1m[32minclude/xinim/simd/math 2.hpp[39m[22m [1:1 - 345:38] (344 lines, 2876 tokens)
   [1m[32minclude/xinim/simd/math.hpp[39m[22m [1:1 - 345:38]

Clone found (cpp-header):
 - [1m[32minclude/xinim/simd/detect 2.hpp[39m[22m [1:1 - 287:25] (286 lines, 2696 tokens)
   [1m[32minclude/xinim/simd/detect.hpp[39m[22m [1:1 - 287:25]

Clone found (cpp-header):
 - [1m[32minclude/xinim/simd/core 2.hpp[39m[22m [1:1 - 385:25] (384 lines, 2961 tokens)
   [1m[32minclude/xinim/simd/core.hpp[39m[22m [1:1 - 385:25]

Clone found (cpp-header):
 - [1m[32minclude/xinim/io/stream 2.hpp[39m[22m [3:1 - 66:23] (63 lines, 423 tokens)
   [1m[32minclude/xinim/io/stream.hpp[39m[22m [3:1 - 66:23]

Clone found (cpp-header):
 - [1m[32minclude/xinim/io/file_operations 2.hpp[39m[22m [3:1 - 55:23] (52 lines, 326 tokens)
   [1m[32minclude/xinim/io/file_operations.hpp[39m[22m [3:1 - 55:23]

```

</details>

<details>
<summary><h2 id="nyc">nyc</h2></summary>

```text
17.1.0
```

</details>
