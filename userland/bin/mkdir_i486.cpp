// mkdir -- make directories (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Options: -p (create parent directories as needed)

#include <xinim/userland/dietlibc_cpp23.hpp>

extern "C" {
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
}

namespace {

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

    void report_error(const char *program_name, const char *message) {
        write_string(2, program_name);
        write_string(2, ": ");
        write_string(2, message);
        write_string(2, "\n");
    }

    void report_path_error(const char *program_name, const char *path, const char *message) {
        write_string(2, program_name);
        write_string(2, ": ");
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

    // Create directory and all parent components.
    // Returns 0 on success, -1 on failure.
    int create_parent_directories(const char *path) {
        char path_buffer[1024];
        const int path_length = string_length(path);
        if (path_length >= static_cast<int>(sizeof(path_buffer))) {
            return -1;
        }

        for (int character_index = 0; character_index < path_length; ++character_index) {
            path_buffer[character_index] = path[character_index];
        }
        path_buffer[path_length] = '\0';

        // Walk through path components and create each
        for (int character_index = 1; character_index <= path_length; ++character_index) {
            if (character_index == path_length || path_buffer[character_index] == '/') {
                const char saved_character = path_buffer[character_index];
                path_buffer[character_index] = '\0';

                struct stat path_status;
                if (stat(path_buffer, &path_status) < 0) {
                    if (mkdir(path_buffer, 0755) < 0 && errno != EEXIST) {
                        return -1;
                    }
                } else if (!S_ISDIR(path_status.st_mode)) {
                    return -1;
                }

                path_buffer[character_index] = saved_character;
            }
        }

        return 0;
    }

} // namespace

extern "C" int xinim_user_main(int argument_count, char **arguments) asm("main");

extern "C" int xinim_user_main(int argument_count, char **arguments) {
    bool create_parents = false;
    int first_operand_index = 1;

    for (int argument_index = 1; argument_index < argument_count; ++argument_index) {
        if (arguments[argument_index][0] == '-' && arguments[argument_index][1] != '\0') {
            for (int option_index = 1; arguments[argument_index][option_index] != '\0';
                 ++option_index) {
                switch (arguments[argument_index][option_index]) {
                case 'p':
                    create_parents = true;
                    break;
                default:
                    report_error("mkdir", "unknown option");
                    return 1;
                }
            }
            first_operand_index = argument_index + 1;
        } else {
            break;
        }
    }

    if (first_operand_index >= argument_count) {
        report_error("mkdir", "missing operand");
        return 1;
    }

    int exit_status = 0;
    for (int argument_index = first_operand_index; argument_index < argument_count;
         ++argument_index) {
        if (create_parents) {
            if (create_parent_directories(arguments[argument_index]) < 0) {
                report_path_error("mkdir", arguments[argument_index], "cannot create directory");
                exit_status = 1;
            }
        } else {
            if (mkdir(arguments[argument_index], 0755) < 0) {
                report_path_error("mkdir", arguments[argument_index], "cannot create directory");
                exit_status = 1;
            }
        }
    }

    return exit_status;
}
