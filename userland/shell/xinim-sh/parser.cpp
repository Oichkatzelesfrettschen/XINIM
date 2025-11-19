/**
 * @file parser.cpp
 * @brief Command line parser for xinim-sh
 *
 * Simple command parser supporting:
 * - Tokenization by whitespace
 * - Background operator (&)
 * - Basic quoting (future)
 *
 * @author XINIM Development Team
 * @date November 2025
 */

#include "shell.hpp"
#include <cstring>
#include <cctype>
#include <cstdlib>

namespace xinim::shell {

/**
 * @brief Parse command line into arguments
 *
 * Tokenizes input string and fills Command structure.
 * Supports background operator (&) at end of line.
 *
 * @param input Command line string
 * @param cmd Command structure to fill
 * @return true on success, false on error
 */
bool parse_command(const char* input, Command* cmd) {
    if (!input || !cmd) {
        return false;
    }

    // Initialize command
    cmd->argc = 0;
    cmd->background = false;
    for (int i = 0; i < MAX_ARGS; i++) {
        cmd->args[i] = nullptr;
    }

    // Skip leading whitespace
    while (*input && isspace(*input)) {
        input++;
    }

    // Empty command
    if (*input == '\0') {
        return false;
    }

    // Duplicate input for tokenization
    char* line = strdup(input);
    if (!line) {
        return false;
    }

    // Check for background operator at end
    char* bg_pos = strrchr(line, '&');
    if (bg_pos) {
        // Make sure it's at the end (ignoring whitespace)
        char* p = bg_pos + 1;
        while (*p && isspace(*p)) p++;

        if (*p == '\0') {
            cmd->background = true;
            *bg_pos = '\0';  // Remove & from command
        }
    }

    // Tokenize by whitespace
    char* token = strtok(line, " \t\n\r");
    while (token && cmd->argc < MAX_ARGS - 1) {
        cmd->args[cmd->argc] = strdup(token);
        if (!cmd->args[cmd->argc]) {
            free(line);
            free_command(cmd);
            return false;
        }
        cmd->argc++;
        token = strtok(nullptr, " \t\n\r");
    }

    // Null-terminate args array
    cmd->args[cmd->argc] = nullptr;

    free(line);
    return cmd->argc > 0;
}

/**
 * @brief Free command structure
 *
 * Frees all allocated argument strings.
 *
 * @param cmd Command to free
 */
void free_command(Command* cmd) {
    if (!cmd) {
        return;
    }

    for (int i = 0; i < cmd->argc; i++) {
        if (cmd->args[i]) {
            free(cmd->args[i]);
            cmd->args[i] = nullptr;
        }
    }

    cmd->argc = 0;
    cmd->background = false;
}

} // namespace xinim::shell
