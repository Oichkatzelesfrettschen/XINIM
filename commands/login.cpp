/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* login - log into the system		Author: Patrick van Kleef */

#include "pwd.hpp"    // password entry handling
#include "sgtty.hpp"  // terminal control
#include "signal.hpp" // signal handling
#include <array>
#include <cstring>
#include <string_view>

int main() {
    // Buffers for the user name and password
    std::array<char, 30> name{};
    std::array<char, 30> pwd_buf{};

    // Temporary variables
    int n = 0;
    int n1 = 0;
    int bad = 0;

    // Terminal settings for password input
    struct sgttyb args{};

    // Password database entry
    struct passwd* pwd = nullptr;

    args.sg_kill = '@';
    args.sg_erase = '\b';
    args.sg_flags = 06030;
    ioctl(0, TIOCSETP, &args);

    /* Get login name and passwd. */
    for (;;) {
        bad = 0;
        do {
            write(1, "login: ", 7);
            n = read(0, name.data(), name.size());
        } while (n < 2);
        name[n - 1] = 0;

        /* Look up login/passwd. */
        pwd = getpwnam(name.data());
        if (pwd == nullptr)
            bad++;

        if (bad || std::strlen(pwd->pw_passwd) != 0) {
            args.sg_flags = 06020;
            ioctl(0, TIOCSETP, &args);
            write(1, "Password: ", 10);
            n1 = read(0, pwd_buf.data(), pwd_buf.size());
            pwd_buf[n1 - 1] = 0;
            write(1, "\n", 1);
            args.sg_flags = 06030;
            ioctl(0, TIOCSETP, &args);
            if (bad || std::strcmp(pwd->pw_passwd, crypt(pwd_buf.data(), pwd->pw_passwd))) {
                write(1, "Login incorrect\n", 16);
                continue;
            }
        }

        /* Successful login. */
        setgid(pwd->pw_gid);
        setuid(pwd->pw_uid);
        chdir(pwd->pw_dir);
        if (pwd->pw_shell) {
            execl(pwd->pw_shell, "-", 0);
        }
        execl("/bin/sh", "-", 0);
        write(1, "exec failure\n", 13);
    }

    return 0; // unreachable but silences compiler warnings
}
