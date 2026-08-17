// rm -- remove files and directories (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -r/-R (recursive), -f (force, no error on missing)

#include <xinim/userland/dietlibc_cpp23.hpp>

extern "C" {
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
}

namespace {

    inline constexpr int kPathCapacity = 1024;

    void write_string(int file_descriptor, const char *text) {
        int remaining_length = 0;
        while (text[remaining_length] != '\0') {
            ++remaining_length;
        }
        while (remaining_length > 0) {
            const auto write_result =
                write(file_descriptor, text, static_cast<unsigned>(remaining_length));
            if (write_result <= 0) {
                return;
            }
            text += write_result;
            remaining_length -= static_cast<int>(write_result);
        }
    }

    void report_error(const char *message) {
        write_string(2, "rm: ");
        write_string(2, message);
        write_string(2, "\n");
    }

    void report_path_error(const char *path, const char *message) {
        write_string(2, "rm: ");
        write_string(2, path);
        write_string(2, ": ");
        write_string(2, message);
        write_string(2, "\n");
    }

    int string_length(const char *text) {
        int length = 0;
        while (text[length] != '\0') {
            ++length;
        }
        return length;
    }

    bool join_path(char *output, int output_capacity, const char *directory,
                   const char *entry_name) {
        const int directory_length = string_length(directory);
        const int entry_name_length = string_length(entry_name);
        const bool separator_needed =
            directory_length > 0 && directory[directory_length - 1] != '/';
        const int required_capacity =
            directory_length + (separator_needed ? 1 : 0) + entry_name_length + 1;
        if (required_capacity > output_capacity) {
            return false;
        }

        int output_index = 0;
        for (int directory_index = 0; directory_index < directory_length; ++directory_index) {
            output[output_index++] = directory[directory_index];
        }
        if (separator_needed) {
            output[output_index++] = '/';
        }
        for (int name_index = 0; name_index < entry_name_length; ++name_index) {
            output[output_index++] = entry_name[name_index];
        }
        output[output_index] = '\0';
        return true;
    }

    int remove_path_recursive(const char *path) {
        struct stat path_status{};
        if (lstat(path, &path_status) < 0) {
            return -1;
        }

        if (S_ISDIR(path_status.st_mode)) {
            auto *directory_stream = opendir(path);
            if (directory_stream == nullptr) {
                return -1;
            }
            int removal_status = 0;
            while (true) {
                errno = 0;
                struct dirent *directory_entry = readdir(directory_stream);
                if (directory_entry == nullptr) {
                    if (errno != 0) {
                        removal_status = -1;
                    }
                    break;
                }
                if (directory_entry->d_name[0] == '.' &&
                    (directory_entry->d_name[1] == '\0' ||
                     (directory_entry->d_name[1] == '.' && directory_entry->d_name[2] == '\0'))) {
                    continue;
                }
                char child_path[kPathCapacity]{};
                if (!join_path(child_path, static_cast<int>(sizeof(child_path)), path,
                               directory_entry->d_name) ||
                    remove_path_recursive(child_path) < 0) {
                    removal_status = -1;
                }
            }
            if (closedir(directory_stream) < 0) {
                removal_status = -1;
            }
            if (rmdir(path) < 0) {
                removal_status = -1;
            }
            return removal_status;
        }

        return unlink(path);
    }

} // namespace

extern "C" int xinim_user_main(int argument_count, char **arguments) asm("main");

extern "C" int xinim_user_main(int argument_count, char **arguments) {
    bool recursive = false;
    bool force = false;
    int first_operand_index = 1;

    for (int argument_index = 1; argument_index < argument_count; ++argument_index) {
        if (arguments[argument_index][0] != '-' || arguments[argument_index][1] == '\0') {
            break;
        }
        if (arguments[argument_index][1] == '-' && arguments[argument_index][2] == '\0') {
            first_operand_index = argument_index + 1;
            break;
        }
        for (int option_index = 1; arguments[argument_index][option_index] != '\0';
             ++option_index) {
            switch (arguments[argument_index][option_index]) {
            case 'r':
            case 'R':
                recursive = true;
                break;
            case 'f':
                force = true;
                break;
            default:
                report_error("unknown option");
                return 1;
            }
        }
        first_operand_index = argument_index + 1;
    }

    if (first_operand_index >= argument_count) {
        if (force) {
            return 0;
        }
        report_error("missing operand");
        return 1;
    }

    int exit_status = 0;
    for (int argument_index = first_operand_index; argument_index < argument_count;
         ++argument_index) {
        const char *path = arguments[argument_index];
        struct stat path_status{};

        if (lstat(path, &path_status) < 0) {
            if (!(force && errno == ENOENT)) {
                report_path_error(path,
                                  errno == ENOENT ? "no such file or directory" : "cannot inspect");
                exit_status = 1;
            }
            continue;
        }

        if (S_ISDIR(path_status.st_mode)) {
            if (!recursive) {
                report_path_error(path, "is a directory (use -r)");
                exit_status = 1;
                continue;
            }
            if (remove_path_recursive(path) < 0) {
                report_path_error(path, "removal failed");
                exit_status = 1;
            }
        } else {
            if (unlink(path) < 0) {
                report_path_error(path, "cannot remove");
                exit_status = 1;
            }
        }
    }

    return exit_status;
}
