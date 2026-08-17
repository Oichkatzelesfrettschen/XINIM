#include <xinim/userland/dietlibc_cpp23.hpp>

extern "C" {
#include <signal.h>
#include <sys/wait.h>
}

namespace {

    using Size = decltype(sizeof(0));

    bool write_all(int file_descriptor, const char *bytes, Size byte_count) noexcept {
        while (byte_count > 0U) {
            constexpr Size maximum_chunk = static_cast<Size>(~0U);
            const Size chunk_size = byte_count < maximum_chunk ? byte_count : maximum_chunk;
            const auto bytes_written =
                write(file_descriptor, bytes, static_cast<unsigned>(chunk_size));
            if (bytes_written <= 0)
                return false;
            const auto completed = static_cast<Size>(bytes_written);
            bytes += completed;
            byte_count -= completed;
        }
        return true;
    }

    void report(const char *message) noexcept {
        Size length = 0U;
        while (message[length] != '\0')
            ++length;
        static_cast<void>(write_all(2, message, length));
    }

    void terminate_child(int process_id) noexcept {
        static_cast<void>(kill(process_id, SIGKILL));
        int status = 0;
        static_cast<void>(waitpid(process_id, &status, 0));
    }

} // namespace

extern "C" int xinim_user_main() asm("main");

extern "C" int xinim_user_main() {
    int descriptors[2];
    if (pipe(descriptors) != 0) {
        report("printf-signal-oracle: pipe failed\n");
        return 1;
    }

    const int child_process_id = fork();
    if (child_process_id < 0) {
        static_cast<void>(close(descriptors[0]));
        static_cast<void>(close(descriptors[1]));
        report("printf-signal-oracle: fork failed\n");
        return 1;
    }
    if (child_process_id == 0) {
        static_cast<void>(close(descriptors[0]));
        if (dup2(descriptors[1], 1) < 0) {
            _exit(126);
        }
        static_cast<void>(close(descriptors[1]));
        char path[] = "/bin/printf";
        char format[] = "%1000000000s";
        char argument[] = "x";
        char *child_arguments[] = {path, format, argument, nullptr};
        execv(path, child_arguments);
        _exit(127);
    }

    static_cast<void>(close(descriptors[1]));
    char observed_byte = '\0';
    const auto bytes_read = read(descriptors[0], &observed_byte, 1U);
    if (bytes_read != 1 || observed_byte != ' ') {
        static_cast<void>(close(descriptors[0]));
        terminate_child(child_process_id);
        report("printf-signal-oracle: child did not execute printf\n");
        return 1;
    }
    if (kill(child_process_id, SIGTERM) != 0) {
        static_cast<void>(close(descriptors[0]));
        terminate_child(child_process_id);
        report("printf-signal-oracle: SIGTERM failed\n");
        return 1;
    }
    static_cast<void>(close(descriptors[0]));

    int child_status = 0;
    if (waitpid(child_process_id, &child_status, 0) != child_process_id) {
        terminate_child(child_process_id);
        report("printf-signal-oracle: waitpid failed\n");
        return 1;
    }
    if (!WIFSIGNALED(child_status) || WTERMSIG(child_status) != SIGTERM) {
        report("printf-signal-oracle: printf did not retain default SIGTERM behavior\n");
        return 1;
    }
    return 0;
}
