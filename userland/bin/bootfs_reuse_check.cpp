#include <xinim/userland/dietlibc_cpp23.hpp>

extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
}

namespace {

    constexpr unsigned int kExpectedDynamicNodeCount = 64U;
    constexpr char kNumberedPathPrefix[] = "/tmp/bootfs-reuse-node-";
    constexpr char kUnlinkedPath[] = "/tmp/bootfs-reuse-unlinked";
    constexpr char kOverflowPath[] = "/tmp/bootfs-reuse-overflow";

    using Size = decltype(sizeof(0));

    [[nodiscard]] Size string_length(const char *text) noexcept {
        Size length = 0U;
        while (text[length] != '\0') {
            ++length;
        }
        return length;
    }

    [[nodiscard]] bool write_all(int descriptor, const char *bytes, Size byte_count) noexcept {
        while (byte_count != 0U) {
            const auto written = write(descriptor, bytes, byte_count);
            if (written <= 0) {
                return false;
            }
            const auto completed = static_cast<Size>(written);
            bytes += completed;
            byte_count -= completed;
        }
        return true;
    }

    void report(const char *message) noexcept {
        static_cast<void>(write_all(2, message, string_length(message)));
    }

    void report_index(const char *prefix, unsigned int value) noexcept {
        char message[96]{};
        Size output_index = 0U;
        while (prefix[output_index] != '\0') {
            message[output_index] = prefix[output_index];
            ++output_index;
        }

        char reversed_digits[16]{};
        Size digit_count = 0U;
        do {
            reversed_digits[digit_count] = static_cast<char>('0' + static_cast<char>(value % 10U));
            value /= 10U;
            ++digit_count;
        } while (value != 0U);
        while (digit_count != 0U) {
            --digit_count;
            message[output_index] = reversed_digits[digit_count];
            ++output_index;
        }
        message[output_index] = '\n';
        ++output_index;
        static_cast<void>(write_all(2, message, output_index));
    }

    void numbered_path(unsigned int node_index, char (&path)[64]) noexcept {
        Size output_index = 0U;
        while (kNumberedPathPrefix[output_index] != '\0') {
            path[output_index] = kNumberedPathPrefix[output_index];
            ++output_index;
        }

        char reversed_digits[16]{};
        Size digit_count = 0U;
        do {
            reversed_digits[digit_count] =
                static_cast<char>('0' + static_cast<char>(node_index % 10U));
            node_index /= 10U;
            ++digit_count;
        } while (node_index != 0U);
        while (digit_count != 0U) {
            --digit_count;
            path[output_index] = reversed_digits[digit_count];
            ++output_index;
        }
        path[output_index] = '\0';
    }

    void remove_numbered_nodes() noexcept {
        for (unsigned int node_index = 0U; node_index < kExpectedDynamicNodeCount; ++node_index) {
            char path[64]{};
            numbered_path(node_index, path);
            static_cast<void>(unlink(path));
        }
        static_cast<void>(unlink(kOverflowPath));
        static_cast<void>(unlink(kUnlinkedPath));
    }

    [[nodiscard]] bool require_full_capacity(const char *failure_prefix) noexcept {
        unsigned int allocated_count = 0U;
        for (; allocated_count < kExpectedDynamicNodeCount; ++allocated_count) {
            char path[64]{};
            numbered_path(allocated_count, path);
            const int descriptor = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
            if (descriptor < 0) {
                report_index(failure_prefix, allocated_count);
                remove_numbered_nodes();
                return false;
            }
            if (close(descriptor) != 0) {
                report("BOOTFS_REUSE_CLOSE_FAILED\n");
                remove_numbered_nodes();
                return false;
            }
        }

        errno = 0;
        const int overflow_descriptor = open(kOverflowPath, O_CREAT | O_EXCL | O_RDWR, 0600);
        if (overflow_descriptor >= 0) {
            static_cast<void>(close(overflow_descriptor));
            report("BOOTFS_REUSE_CAPACITY_EXCEEDED\n");
            remove_numbered_nodes();
            return false;
        }
        if (errno != ENOSPC) {
            report("BOOTFS_REUSE_OVERFLOW_ERRNO_FAILED\n");
            remove_numbered_nodes();
            return false;
        }

        remove_numbered_nodes();
        return true;
    }

    [[nodiscard]] bool require_open_unlinked_lifecycle() noexcept {
        for (unsigned int iteration = 0U; iteration < kExpectedDynamicNodeCount; ++iteration) {
            const int original_descriptor = open(kUnlinkedPath, O_CREAT | O_EXCL | O_RDWR, 0600);
            if (original_descriptor < 0) {
                report_index("BOOTFS_REUSE_UNLINKED_OPEN_FAILED_", iteration);
                return false;
            }
            const int duplicate_descriptor = dup(original_descriptor);
            if (duplicate_descriptor < 0 || unlink(kUnlinkedPath) != 0) {
                report_index("BOOTFS_REUSE_UNLINK_DUP_FAILED_", iteration);
                static_cast<void>(close(original_descriptor));
                if (duplicate_descriptor >= 0) {
                    static_cast<void>(close(duplicate_descriptor));
                }
                return false;
            }

            const pid_t child_process = fork();
            if (child_process < 0) {
                report_index("BOOTFS_REUSE_FORK_FAILED_", iteration);
                static_cast<void>(close(original_descriptor));
                static_cast<void>(close(duplicate_descriptor));
                return false;
            }
            if (child_process == 0) {
                static_cast<void>(close(original_descriptor));
                _exit(0);
            }

            if (close(original_descriptor) != 0 || close(duplicate_descriptor) != 0) {
                report_index("BOOTFS_REUSE_PARENT_CLOSE_FAILED_", iteration);
                return false;
            }
            int wait_status = 0;
            if (waitpid(child_process, &wait_status, 0) != child_process ||
                !WIFEXITED(wait_status) || WEXITSTATUS(wait_status) != 0) {
                report_index("BOOTFS_REUSE_WAIT_FAILED_", iteration);
                return false;
            }

            const int replacement_descriptor = open(kUnlinkedPath, O_CREAT | O_EXCL | O_RDWR, 0600);
            if (replacement_descriptor < 0 || close(replacement_descriptor) != 0 ||
                unlink(kUnlinkedPath) != 0) {
                report_index("BOOTFS_REUSE_REPLACEMENT_FAILED_", iteration);
                return false;
            }
        }
        return true;
    }

} // namespace

int main() {
    remove_numbered_nodes();
    if (!require_full_capacity("BOOTFS_REUSE_INITIAL_CAPACITY_")) {
        return 1;
    }
    if (!require_open_unlinked_lifecycle()) {
        return 1;
    }
    if (!require_full_capacity("BOOTFS_REUSE_FINAL_CAPACITY_")) {
        return 1;
    }
    report("BOOTFS_DYNAMIC_NODE_REUSE_OK\n");
    return 0;
}
