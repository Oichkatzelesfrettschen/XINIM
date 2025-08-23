// include/xinim/mode_parser.hpp
#pragma once

#include <filesystem>
#include <expected>
#include <string_view>
#include <string>       // For std::string in ParseResult and error messages
#include <vector>       // Not directly used by ModeParser, but often by consumers
#include <algorithm>    // For std::ranges::all_of
#include <ranges>       // For std::ranges::all_of (C++20)
#include <charconv>     // For std::from_chars (C++17)
#include <system_error> // For std::errc
#include <optional>     // For std::optional (not directly used in this version, but good for utils)

// Required for std::filesystem::perms definitions
// and potentially other utilities if this header becomes more general.

namespace xinim::util {

class ModeParser {
public:
    // Using std::expected for returning either perms or an error string.
    using ParseResult = std::expected<std::filesystem::perms, std::string>;

    /**
     * @brief Parses an octal or symbolic mode string.
     * @param mode_str The mode string (e.g., "755", "u+x,go=r").
     * @param current_perms The current permissions of the file/directory, needed for symbolic modes.
     * @return A ParseResult containing the new permissions on success, or an error string on failure.
     */
    ParseResult parse(std::string_view mode_str,
                     std::filesystem::perms current_perms) const {
        if (mode_str.empty()) {
            return std::unexpected(std::string("Empty mode string"));
        }

        // Determine if octal or symbolic.
        bool is_octal_candidate = !mode_str.empty() &&
                                  std::ranges::all_of(mode_str, [](char c) { return c >= '0' && c <= '7'; });

        if (is_octal_candidate) {
            return parse_octal(mode_str);
        } else {
            // If not purely octal, try parsing as symbolic.
            return parse_symbolic(mode_str, current_perms);
        }
    }

private:
    ParseResult parse_octal(std::string_view mode_str) const {
        unsigned int mode_value = 0;
        auto [ptr, ec] = std::from_chars(mode_str.data(), mode_str.data() + mode_str.size(),
                                        mode_value, 8); // Base 8 for octal

        if (ec != std::errc{} || ptr != mode_str.data() + mode_str.size()) {
            return std::unexpected(std::string("Invalid octal mode: '") + std::string(mode_str) + "'");
        }
        // POSIX permissions typically go up to 07777 (including setuid, setgid, sticky bit).
        if (mode_value > 07777) {
            return std::unexpected(std::string("Octal mode value too large: '") + std::string(mode_str) + "' (max 07777)");
        }

        // Convert the integer mode_value to std::filesystem::perms
        return static_cast<std::filesystem::perms>(mode_value);
    }

    // Helper for parse_symbolic_clause to get permission bits from 'r','w','x','s','t'
    // For 's', it returns a combined mask that might need filtering based on 'who'.
    std::filesystem::perms get_permission_char_mask(char perm_char) const {
        switch (perm_char) {
            case 'r': return std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read;
            case 'w': return std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
            case 'x': return std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
            case 's': return std::filesystem::perms::set_uid | std::filesystem::perms::set_gid; // 's' potentially affects both
            case 't': return std::filesystem::perms::sticky_bit;
            // 'X' (execute if directory or some execute bit already set) is more complex.
            // Copying permissions from another class (u,g,o) also not handled here.
            default: return std::filesystem::perms::unknown; // Mark as invalid
        }
    }

    ParseResult parse_symbolic_clause(std::string_view clause,
                                      std::filesystem::perms /*current_perms_for_context*/, // current_perms not directly used here for +,-,= with r,w,x,s,t; needed for 'X'
                                      std::filesystem::perms& perms_accumulator) const { // Accumulator is modified
        size_t op_pos = clause.find_first_of("+-=");
        if (op_pos == std::string_view::npos ) { // Operator must exist
             return std::unexpected(std::string("Symbolic mode clause '") + std::string(clause) + "' missing operator.");
        }
        // Operator at start (e.g. "+x") is allowed by some chmod, implies 'a' for who.
        // This parser makes 'who' optional, defaulting to 'a' if op_pos is 0.

        char op = clause[op_pos];
        std::string_view who_str = clause.substr(0, op_pos);
        std::string_view what_str = clause.substr(op_pos + 1);

        // Determine the 'who' mask (which permission categories are affected)
        std::filesystem::perms who_target_mask = std::filesystem::perms::none;
        if (who_str.empty()) { // Default to 'a' (umask is not handled by parser)
            who_target_mask = std::filesystem::perms::owner_all | std::filesystem::perms::group_all | std::filesystem::perms::others_all |
                              std::filesystem::perms::set_uid | std::filesystem::perms::set_gid | std::filesystem::perms::sticky_bit;
        } else {
            for (char c : who_str) {
                switch (c) {
                    case 'u': who_target_mask |= (std::filesystem::perms::owner_all | std::filesystem::perms::set_uid); break;
                    case 'g': who_target_mask |= (std::filesystem::perms::group_all | std::filesystem::perms::set_gid); break;
                    case 'o': who_target_mask |= (std::filesystem::perms::others_all | std::filesystem::perms::sticky_bit); break; // 'o' can affect sticky bit for some interpretations
                    case 'a': who_target_mask |= (std::filesystem::perms::all | std::filesystem::perms::set_uid | std::filesystem::perms::set_gid | std::filesystem::perms::sticky_bit); break;
                    default: return std::unexpected(std::string("Invalid 'who' character '") + c + "' in symbolic mode: '" + std::string(clause) + "'");
                }
            }
        }

        std::filesystem::perms perms_from_what_str = std::filesystem::perms::none;
        if (what_str.empty() && op != '=') { // e.g. "u+", "g-" - perms_from_what_str remains none
            // This is valid, means no change to permissions bits unless op is '=' (clears specific bits).
        } else { // what_str is not empty or op is '='
            for (char c : what_str) {
                std::filesystem::perms p_bit = get_permission_char_mask(c);
                if (p_bit == std::filesystem::perms::unknown) {
                     return std::unexpected(std::string("Invalid permission character '") + c + "' in symbolic mode: '" + std::string(clause) + "'");
                }
                perms_from_what_str |= p_bit;
            }
        }

        // Apply 'who' mask to the permissions derived from 'what_str'
        // For example, if what_str is "rwx" (which translates to perms_owner_all | perms_group_all | perms_others_all)
        // and who_str is "u", then effective_perms_to_change should only be perms_owner_all.
        std::filesystem::perms effective_perms_to_change = perms_from_what_str & who_target_mask;


        switch (op) {
            case '+':
                perms_accumulator |= effective_perms_to_change;
                break;
            case '-':
                perms_accumulator &= ~effective_perms_to_change;
                break;
            case '=':
                perms_accumulator &= ~who_target_mask; // Clear all bits for the 'who' selection
                perms_accumulator |= effective_perms_to_change;
                break;
            default: // Should not be reached if op_pos check is good, but as a safeguard.
                return std::unexpected(std::string("Invalid operator '") + op + "' in symbolic mode: '" + std::string(clause) + "'");
        }
        return perms_accumulator; // Return success with modified accumulator
    }

    ParseResult parse_symbolic(std::string_view mode_str,
                               std::filesystem::perms current_perms) const {
        std::filesystem::perms calculated_perms = current_perms; // Start with current perms
        size_t start = 0;

        while(start < mode_str.length()) {
            size_t comma_pos = mode_str.find(',', start);
            std::string_view clause_str;
            if (comma_pos == std::string_view::npos) {
                clause_str = mode_str.substr(start);
                start = mode_str.length();
            } else {
                clause_str = mode_str.substr(start, comma_pos - start);
                start = comma_pos + 1;
            }

            if (clause_str.empty()) {
                if (start >= mode_str.length() && comma_pos != std::string_view::npos) {
                    break; // Trailing comma is okay
                }
                return std::unexpected(std::string("Empty clause in symbolic mode string."));
            }

            auto clause_parse_result = parse_symbolic_clause(clause_str, current_perms, calculated_perms);
            if (!clause_parse_result) {
                return clause_parse_result;
            }
            // calculated_perms has been updated by reference.
        }
        return calculated_perms;
    }
};

} // namespace xinim::util
