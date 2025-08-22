# Analysis Tree Fresh

```text
.
├── analysis_cloc_detailed_fresh.txt
├── analysis_cloc_detailed.txt
├── analysis_cloc_fresh_detailed.txt
├── analysis_cloc_fresh.txt
├── analysis_cloc.txt
├── analysis_cppcheck_fresh.xml
├── analysis_cppcheck.txt
├── analysis_cscope_fresh.txt
├── analysis_cscope_summary.txt
├── analysis_ctags_fresh.txt
├── analysis_ctags_summary.txt
├── analysis_flawfinder_fresh.txt
├── analysis_flawfinder.txt
├── analysis_inline_asm_fresh.txt
├── analysis_inline_asm.txt
├── analysis_lizard_fresh_detailed.txt
├── analysis_lizard_fresh.txt
├── analysis_lizard.txt
├── analysis_pmccabe_fresh.txt
├── analysis_pmccabe.txt
├── analysis_radon_cc.txt
├── analysis_radon_mi.txt
├── analysis_sloccount_fresh.txt
├── analysis_sloccount.txt
├── analysis_tree_fresh.txt
├── boot.S
├── .clang-format
├── .clang-tidy
├── CMakeLists.txt
├── commands
│   ├── ar.cpp
│   ├── basename.cpp
│   ├── c86
│   │   └── make.bat
│   ├── cal.cpp
│   ├── cat.cpp
│   ├── cc.cpp
│   ├── chmem.cpp
│   ├── chmod.cpp
│   ├── chown.cpp
│   ├── clr.cpp
│   ├── CMakeLists.txt
│   ├── cmp.cpp
│   ├── comm.cpp
│   ├── cp.cpp
│   ├── date.cpp
│   ├── dd.cpp
│   ├── df.cpp
│   ├── dosread.cpp
│   ├── echo.cpp
│   ├── getlf.cpp
│   ├── grep.cpp
│   ├── gres.cpp
│   ├── head.cpp
│   ├── kill.cpp
│   ├── libpack.cpp
│   ├── libupack.cpp
│   ├── ln.cpp
│   ├── login.cpp
│   ├── lpr.cpp
│   ├── ls.cpp
│   ├── make.cpp
│   ├── Makefile
│   ├── mined1.cpp
│   ├── mined2.cpp
│   ├── mined.hpp
│   ├── mkdir.cpp
│   ├── mkfs.cpp
│   ├── mknod.cpp
│   ├── mount.cpp
│   ├── mv.cpp
│   ├── od.cpp
│   ├── passwd.cpp
│   ├── pr.cpp
│   ├── pwd.cpp
│   ├── rev.cpp
│   ├── rm.cpp
│   ├── rmdir.cpp
│   ├── roff.cpp
│   ├── sh1.cpp
│   ├── sh2.cpp
│   ├── sh3.cpp
│   ├── sh4.cpp
│   ├── sh5.cpp
│   ├── shar.cpp
│   ├── size.cpp
│   ├── sleep.cpp
│   ├── sort.cpp
│   ├── split.cpp
│   ├── stty.cpp
│   ├── su.cpp
│   ├── sum.cpp
│   ├── svcctl.cpp
│   ├── svcctl.hpp
│   ├── sync.cpp
│   ├── tail.cpp
│   ├── tar.cpp
│   ├── tee.cpp
│   ├── time.cpp
│   ├── touch.cpp
│   ├── tr.cpp
│   ├── umount.cpp
│   ├── uniq.cpp
│   ├── update.cpp
│   ├── wc.cpp
│   └── x.cpp
├── common
│   └── math
│       ├── Makefile
│       ├── octonion.cpp
│       ├── octonion.hpp
│       ├── quaternion.cpp
│       ├── quaternion.hpp
│       ├── sedenion.cpp
│       └── sedenion.hpp
├── console.cpp
├── console.h
├── crypto
│   ├── CMakeLists.txt
│   ├── kyber.cpp
│   ├── kyber.hpp
│   ├── kyber_impl
│   │   ├── api.h
│   │   ├── cbd.c
│   │   ├── cbd.h
│   │   ├── fips202.c
│   │   ├── fips202.h
│   │   ├── indcpa.c
│   │   ├── indcpa.h
│   │   ├── kem.c
│   │   ├── kem.h
│   │   ├── ntt.c
│   │   ├── ntt.h
│   │   ├── params.h
│   │   ├── poly.c
│   │   ├── poly.h
│   │   ├── polyvec.c
│   │   ├── polyvec.h
│   │   ├── randombytes.c
│   │   ├── randombytes.h
│   │   ├── reduce.c
│   │   ├── reduce.h
│   │   ├── symmetric.h
│   │   ├── symmetric-shake.c
│   │   ├── verify.c
│   │   └── verify.h
│   └── pqcrypto_shared.cpp
├── cscope.files
├── cscope.in.out
├── cscope.out
├── cscope.po.out
├── .devcontainer
│   ├── devcontainer.json
│   ├── Dockerfile
│   └── reinstall-cmake.sh
├── docs
│   ├── AGENTS.md
│   ├── BUILDING.md
│   ├── Doxyfile
│   ├── README_renamed.md
│   ├── ROADMAP.md
│   ├── sphinx
│   │   ├── api.rst
│   │   ├── architecture.rst
│   │   ├── conf.py
│   │   ├── hypercomplex.rst
│   │   ├── index.rst
│   │   ├── lattice_ipc.rst
│   │   ├── libsodium_tests.rst
│   │   ├── networking.rst
│   │   ├── pq_crypto.rst
│   │   ├── precommit.rst
│   │   ├── scheduler.rst
│   │   ├── service_manager.rst
│   │   ├── service.rst
│   │   ├── _static
│   │   │   └── .gitkeep
│   │   └── wait_graph.rst
│   ├── TREE_CLASSIFIED.txt
│   └── TREE.txt
├── fs
│   ├── buf.hpp
│   ├── c86
│   │   ├── _link.bat
│   │   ├── linklist
│   │   └── make.bat
│   ├── cache.cpp
│   ├── CMakeLists.txt
│   ├── compat.cpp
│   ├── compat.hpp
│   ├── const.hpp
│   ├── dev.hpp
│   ├── device.cpp
│   ├── extent.hpp
│   ├── filedes.cpp
│   ├── file.hpp
│   ├── fproc.hpp
│   ├── glo.hpp
│   ├── inode.cpp
│   ├── inode.hpp
│   ├── link.cpp
│   ├── main.cpp
│   ├── minix
│   │   ├── Makefile
│   │   └── makefile.at
│   ├── misc.cpp
│   ├── mount.cpp
│   ├── open.cpp
│   ├── param.hpp
│   ├── path.cpp
│   ├── pipe.cpp
│   ├── protect.cpp
│   ├── putc.cpp
│   ├── read.cpp
│   ├── stadir.cpp
│   ├── super.cpp
│   ├── super.hpp
│   ├── table.cpp
│   ├── time.cpp
│   ├── type.hpp
│   ├── utility.cpp
│   └── write.cpp
├── .gitattributes
├── .github
│   ├── dependabot.yml
│   └── workflows
│       ├── build.yml
│       └── docs.yml
├── .gitignore
├── grub.cfg
├── h
│   ├── callnr.hpp
│   ├── com.hpp
│   ├── const.hpp
│   ├── error.hpp
│   ├── signal.h
│   ├── signal.hpp
│   ├── stat.hpp
│   └── type.hpp
├── include
│   ├── blocksiz.hpp
│   ├── constant_time.hpp
│   ├── ctype.hpp
│   ├── defs.hpp
│   ├── errno.hpp
│   ├── grp.hpp
│   ├── lib.hpp
│   ├── mined.hpp
│   ├── minix
│   │   ├── fs
│   │   │   ├── buffer.hpp
│   │   │   ├── const.hpp
│   │   │   └── inode.hpp
│   │   ├── fs_error.hpp
│   │   └── io
│   │       ├── file_operations.hpp
│   │       ├── file_stream.hpp
│   │       ├── memory_stream.hpp
│   │       ├── standard_streams.hpp
│   │       ├── stdio_compat.hpp
│   │       └── stream.hpp
│   ├── number_parse.hpp
│   ├── paging.hpp
│   ├── pqcrypto.hpp
│   ├── psd
│   │   └── vm
│   │       └── semantic_memory.hpp
│   ├── pwd.hpp
│   ├── regexp.hpp
│   ├── setjmp.hpp
│   ├── sgtty.hpp
│   ├── shared
│   │   ├── number_to_ascii.hpp
│   │   ├── signal_constants.hpp
│   │   └── stat_struct.hpp
│   ├── sh.hpp
│   ├── signal.hpp
│   ├── stat.hpp
│   ├── stdio.hpp
│   ├── vm.hpp
│   └── xinim
│       └── core_types.hpp
├── kernel
│   ├── at_wini.cpp
│   ├── clock.cpp
│   ├── CMakeLists.txt
│   ├── const.hpp
│   ├── dmp.cpp
│   ├── fano_octonion.hpp
│   ├── floppy.cpp
│   ├── glo.hpp
│   ├── idt64.cpp
│   ├── klib64.cpp
│   ├── klib88.cpp
│   ├── lattice_ipc.cpp
│   ├── lattice_ipc.hpp
│   ├── _link.bat
│   ├── linklist
│   ├── main.cpp
│   ├── make.bat
│   ├── Makefile
│   ├── memory.cpp
│   ├── minix
│   │   ├── Makefile
│   │   └── makefile.at
│   ├── mpx64.cpp
│   ├── mpx88.cpp
│   ├── net_driver.cpp
│   ├── net_driver.hpp
│   ├── octonion.hpp
│   ├── octonion_math.hpp
│   ├── paging.cpp
│   ├── pqcrypto.cpp
│   ├── pqcrypto.hpp
│   ├── printer.cpp
│   ├── proc.cpp
│   ├── proc.hpp
│   ├── quaternion_spinlock.hpp
│   ├── schedule.cpp
│   ├── schedule.hpp
│   ├── sedenion.hpp
│   ├── service.cpp
│   ├── service.hpp
│   ├── syscall.cpp
│   ├── system.cpp
│   ├── table.cpp
│   ├── tty.cpp
│   ├── type.hpp
│   ├── wait_graph.cpp
│   ├── wait_graph.hpp
│   ├── wini.cpp
│   ├── wormhole.cpp
│   ├── wormhole.hpp
│   └── xt_wini.cpp
├── kernel.cpp
├── kr_cpp_summary.json
├── lib
│   ├── abort.cpp
│   ├── abs.cpp
│   ├── access.cpp
│   ├── alarm.cpp
│   ├── atoi.cpp
│   ├── atol.cpp
│   ├── bcopy.cpp
│   ├── brk2.cpp
│   ├── brk.cpp
│   ├── brksize.cpp
│   ├── c86
│   │   ├── make.bat
│   │   └── prologue.hpp
│   ├── call.cpp
│   ├── catchsig.cpp
│   ├── chdir.cpp
│   ├── chmod.cpp
│   ├── chown.cpp
│   ├── chroot.cpp
│   ├── cleanup.cpp
│   ├── close.cpp
│   ├── CMakeLists.txt
│   ├── creat.cpp
│   ├── crt0.cpp
│   ├── crtso.cpp
│   ├── crypt.cpp
│   ├── csv.cpp
│   ├── ctype.cpp
│   ├── doprintf.cpp
│   ├── doscanf.cpp
│   ├── dup2.cpp
│   ├── dup.cpp
│   ├── end.cpp
│   ├── exec.cpp
│   ├── exit.cpp
│   ├── fclose.cpp
│   ├── fflush.cpp
│   ├── fgets.cpp
│   ├── fopen.cpp
│   ├── fork.cpp
│   ├── fprintf.cpp
│   ├── fputs.cpp
│   ├── fread.cpp
│   ├── freopen.cpp
│   ├── fseek.cpp
│   ├── fstat.cpp
│   ├── ftell.cpp
│   ├── fwrite.cpp
│   ├── getc.cpp
│   ├── getegid.cpp
│   ├── getenv.cpp
│   ├── geteuid.cpp
│   ├── getgid.cpp
│   ├── getgrent.cpp
│   ├── getpass.cpp
│   ├── getpid.cpp
│   ├── getpwent.cpp
│   ├── gets.cpp
│   ├── getuid.cpp
│   ├── getutil.cpp
│   ├── head.cpp
│   ├── index.cpp
│   ├── io
│   │   └── src
│   │       ├── file_operations.cpp
│   │       ├── file_stream.cpp
│   │       ├── memory_stream.cpp
│   │       ├── standard_streams.cpp
│   │       └── stdio_compat.cpp
│   ├── ioctl.cpp
│   ├── isatty.cpp
│   ├── itoa.cpp
│   ├── kill.cpp
│   ├── link.cpp
│   ├── lseek.cpp
│   ├── Makefile
│   ├── malloc.cpp
│   ├── message.cpp
│   ├── minix
│   │   ├── crtso.cpp
│   │   ├── end.cpp
│   │   ├── head.cpp
│   │   └── setjmp.cpp
│   ├── mknod.cpp
│   ├── mktemp.cpp
│   ├── mount.cpp
│   ├── open.cpp
│   ├── pause.cpp
│   ├── perror.cpp
│   ├── pipe.cpp
│   ├── printdat.cpp
│   ├── printk.cpp
│   ├── prints.cpp
│   ├── putc.cpp
│   ├── rand.cpp
│   ├── read.cpp
│   ├── regexp.cpp
│   ├── regsub.cpp
│   ├── rindex.cpp
│   ├── run
│   ├── safe_alloc.cpp
│   ├── scanf.cpp
│   ├── sendrec.cpp
│   ├── setbuf.cpp
│   ├── setgid.cpp
│   ├── setjmp.cpp
│   ├── setuid.cpp
│   ├── signal.cpp
│   ├── sleep.cpp
│   ├── sprintf.cpp
│   ├── stat.cpp
│   ├── stb.cpp
│   ├── stderr.cpp
│   ├── stime.cpp
│   ├── strcat.cpp
│   ├── strcmp.cpp
│   ├── strcpy.cpp
│   ├── strlen.cpp
│   ├── strncat.cpp
│   ├── strncmp.cpp
│   ├── strncpy.cpp
│   ├── sync.cpp
│   ├── syscall_x86_64.cpp
│   ├── syslib.cpp
│   ├── time.cpp
│   ├── times.cpp
│   ├── umask.cpp
│   ├── umount.cpp
│   ├── ungetc.cpp
│   ├── unlink.cpp
│   ├── utime.cpp
│   ├── wait.cpp
│   └── write.cpp
├── LICENSE
├── limine
├── linker.ld
├── microwindows
├── mm
│   ├── alloc.cpp
│   ├── alloc.hpp
│   ├── break.cpp
│   ├── c86
│   │   ├── _link.bat
│   │   ├── linklist
│   │   └── make.bat
│   ├── CMakeLists.txt
│   ├── const.hpp
│   ├── exec.cpp
│   ├── forkexit.cpp
│   ├── getset.cpp
│   ├── glo.hpp
│   ├── main.cpp
│   ├── Makefile
│   ├── minix
│   │   ├── Makefile
│   │   └── makefile.at
│   ├── mproc.hpp
│   ├── paging.cpp
│   ├── param.hpp
│   ├── putc.cpp
│   ├── signal.cpp
│   ├── table.cpp
│   ├── token.hpp
│   ├── type.hpp
│   ├── utility.cpp
│   └── vm.cpp
├── multiboot.h
├── pmm.cpp
├── pmm.h
├── tags
├── test
│   ├── c86
│   │   └── nullfile
│   ├── Makefile
│   ├── minix
│   │   └── Makefile
│   ├── run
│   ├── t10a.cpp
│   ├── t11a.cpp
│   ├── t11b.cpp
│   ├── t15a.cpp
│   ├── t16a.cpp
│   ├── t16b.cpp
│   ├── test0.cpp
│   ├── test10.cpp
│   ├── test11.cpp
│   ├── test12.cpp
│   ├── test1.cpp
│   ├── test2.cpp
│   ├── test3.cpp
│   ├── test4.cpp
│   ├── test5.cpp
│   ├── test6.cpp
│   ├── test7.cpp
│   ├── test8.cpp
│   └── test9.cpp
├── tests
│   ├── CMakeLists.txt
│   ├── crypto
│   │   ├── CMakeLists.txt
│   │   ├── test_constant_time_equal.cpp
│   │   ├── test_kyber.cpp
│   │   └── test_shared_secret_failure.cpp
│   ├── randombytes_stub.c
│   ├── sodium.h
│   ├── sodium_stub.cpp
│   ├── task_stubs.cpp
│   ├── test_fastpath_cache_performance.cpp
│   ├── test_fastpath.cpp
│   ├── test_fastpath_fallback.cpp
│   ├── test_fastpath_preconditions.cpp
│   ├── test_hypercomplex.cpp
│   ├── test_lattice_blocking.cpp
│   ├── test_lattice.cpp
│   ├── test_lattice_ipc.cpp
│   ├── test_lattice_ipv6.cpp
│   ├── test_lattice_network.cpp
│   ├── test_lattice_network_encrypted.cpp
│   ├── test_lattice_send_error.cpp
│   ├── test_lattice_send_recv.cpp
│   ├── test_lib.cpp
│   ├── test_memory_stream.cpp
│   ├── test_net_driver_concurrency.cpp
│   ├── test_net_driver.cpp
│   ├── test_net_driver_drop_newest.cpp
│   ├── test_net_driver_id.cpp
│   ├── test_net_driver_ipv6.cpp
│   ├── test_net_driver_loopback.cpp
│   ├── test_net_driver_overflow.cpp
│   ├── test_net_driver_persistent_id.cpp
│   ├── test_net_driver_reconnect.cpp
│   ├── test_net_driver_socket_failure.cpp
│   ├── test_net_driver_tcp.cpp
│   ├── test_net_driver_unpriv_id.cpp
│   ├── test_net_two_node.cpp
│   ├── test_poll_network.cpp
│   ├── test_scheduler.cpp
│   ├── test_scheduler_deadlock.cpp
│   ├── test_scheduler_edge.cpp
│   ├── test_semantic_region.cpp
│   ├── test_service_contract.cpp
│   ├── test_service_manager_dag.cpp
│   ├── test_service_manager_updates.cpp
│   ├── test_service_serialization.cpp
│   ├── test_stream_foundation.cpp
│   ├── test_streams.cpp
│   ├── test_svcctl.cpp
│   ├── test_syscall.cpp
│   └── test_wait_graph.cpp
├── tools
│   ├── arch_scan.py
│   ├── ascii_tree.py
│   ├── bootblok1.cpp
│   ├── build.cpp
│   ├── c86
│   │   ├── _bootblo.bat
│   │   ├── bootblok
│   │   ├── _build.bat
│   │   ├── _dos2out.bat
│   │   ├── dos2out.cpp
│   │   ├── _fsck.bat
│   │   ├── _image.bat
│   │   ├── _init.bat
│   │   ├── make.bat
│   │   └── _mkfs.bat
│   ├── changeme
│   ├── classify_style.py
│   ├── CMakeLists.txt
│   ├── diskio.cpp
│   ├── diskio.hpp
│   ├── find_knr.py
│   ├── fsck.cpp
│   ├── generate_kr_summary.py
│   ├── getcore.cpp
│   ├── init.cpp
│   ├── Makefile
│   ├── migration_dashboard.py
│   ├── minix
│   │   ├── Makefile
│   │   └── makefile.at
│   ├── mkfs.cpp
│   ├── modernize_cpp17.sh
│   ├── modernize_kr_file.py
│   ├── passwd
│   ├── pre-commit-clang-format.sh
│   ├── proto.dis
│   ├── proto.ram
│   ├── proto.use
│   ├── proto.usr
│   ├── rc.at
│   ├── r.cpp
│   ├── README 2.md
│   ├── README.md
│   ├── run
│   ├── run_clang_tidy.sh
│   ├── run_cppcheck.sh
│   ├── setup1
│   ├── setup2
│   ├── setup3
│   ├── setup.sh
│   ├── ttys
│   └── versions
├── vmm.cpp
├── vmm.h
└── .vscode
    ├── launch.json
    ├── settings.json
    └── tasks.json

46 directories, 600 files

```
