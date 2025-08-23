// umount.cpp
// A more complete C++23 implementation of umount(1)
// Builds on Linux x86/ARM etc.
// clang++ -std=c++23 umount.cpp -o umount

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/mount.h>  // umount(), umount2(), MNT_DETACH
#include <unistd.h>
#include <vector>
#if !defined(__linux__)
#error "This umount implementation requires Linux."
#endif
#ifndef MNT_DETACH
#define MNT_DETACH 2
#endif

namespace {

struct MountEntry {
    std::string mountPoint;
    std::string fsType;
};

// Print usage and exit
[[noreturn]] void usage() {
    std::cerr <<
        "Usage: umount [options] [<target>...]\n"
        "Options:\n"
        "  -a            unmount all filesystems\n"
        "  -t <fstype>   limit unmount to given filesystem type\n"
        "  -l, --lazy    lazy unmount (detach)\n"
        "  -v            verbose\n"
        "  -h, --help    this help text\n";
    std::exit(EXIT_FAILURE);
}

// Print a nice error message
void print_error(std::string_view target) {
    switch (errno) {
        case EINVAL: std::cerr << target << ": not a mount point\n"; break;
        case EPERM:  std::cerr << target << ": permission denied\n";   break;
        case EBUSY:  std::cerr << target << ": target is busy\n";     break;
        default:     std::cerr << "umount: " << std::strerror(errno) << "\n";
    }
}

// Parse /proc/self/mountinfo into a vector<MountEntry>
std::vector<MountEntry> read_mounts() {
    std::ifstream f("/proc/self/mountinfo");
    if (!f) {
        std::perror("Failed to open /proc/self/mountinfo");
        std::exit(EXIT_FAILURE);
    }
    std::vector<MountEntry> mounts;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string field;
        // skip first 4 fields: ID, parent, dev, root
        for (int i = 0; i < 4; ++i) iss >> field;
        // 5th is mount point
        std::string mnt;
        iss >> std::ws;
        std::getline(iss, mnt, ' ');
        // skip mount options
        iss >> field;
        // skip optional fields until we hit "-"
        while (iss >> field && field != "-") {}
        // next is fs type
        std::string fs, src, opts;
        iss >> fs >> src >> opts;
        mounts.push_back({std::move(mnt), std::move(fs)});
    int flags = lazy ? MNT_DETACH : 0;
    int ret = -1;
    if (lazy) {
#ifdef __NR_umount2
        ret = umount2(target.c_str(), flags);
#else
        std::cerr << "umount2 not available on this system\n";
        return false;
#endif
    } else {
        ret = umount(target.c_str());
    }
    if (ret < 0) {
        print_error(target);
        return false;
    }
    if (verbose) {
        std::cout << "unmounted " << target << "\n";
    }
    return true;
}
    }
    if (verbose) {
        std::cout << "unmounted " << target << "\n";
    }
    return true;
}
    while ((c = getopt_long(argc, argv, "alt:vh", longopts, nullptr)) != -1) {
        switch (c) {
        case 'a': all = true; break;
        case 'l': lazy = true; break;
        case 'v': verbose = true; break;
        case 't':
            if (optarg) fstype = optarg;
            else usage();
            break;
        case 'h':
            usage();
            break;
        default:
            usage();
        }
    }
    };

    int c;
    while ((c = getopt_long(argc, argv, "alt:vh", longopts, nullptr)) != -1) {
        switch (c) {
        case 'a': all = true; break;
        case 'l': lazy = true; break;
        case 'v': verbose = true; break;
        case 't': fstype = optarg; break;
        case 'h':
        default:
            usage();
        }
    }
    std::vector<std::string> targets;
    for (int i = optind; i < argc; ++i) {
        targets.emplace_back(argv[i]);
    }

    if (all) {
        // We ignore explicit targets if -a
        auto mounts = read_mounts();
        // filter by fstype if requested
        if (!fstype.empty()) {
            mounts.erase(
                std::remove_if(mounts.begin(),
                               mounts.end(),
                               [&](auto &m){ return m.fsType != fstype; }),
                mounts.end()
            );
        }
        // Sort by descending mount point length so children unmount before parents
        std::sort(mounts.begin(), mounts.end(),
                  [](auto &a, auto &b){
                      return a.mountPoint.size() > b.mountPoint.size();
                  });
        bool ok = true;
        for (auto &m: mounts) {
            if (!do_unmount(m.mountPoint, lazy, verbose)) {
                ok = false;
            }
        }
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // no -a: must have at least one target
    if (targets.empty()) {
        usage();
    }
    bool ok = true;
    for (auto &t: targets) {
        if (!do_unmount(t, lazy, verbose)) {
            ok = false;
        }
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
