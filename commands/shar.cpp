/// \file shar.cpp
/// @brief Generate a shell archive from one or more files.
///
/// This utility reproduces the classic `shar` program using modern
/// C++23 facilities. Each input file is emitted as a sequence of shell
/// commands that reconstruct the file when executed. Lines are prefixed
/// with the character `X` so that a simple `gres` command can strip the
/// prefix during extraction.
///
/// @section Features
/// - **Modern C++23**: Leverages `std::filesystem`, `std::string_view`, and `std::iostream` for robust I/O.
/// - **RAII**: Ensures proper resource management for file streams.
/// - **Type Safety**: Uses strong types and views to prevent common C-style string errors.
/// - **Simplicity**: Retains the straightforward logic of the original `shar` utility.
///
/// @section Usage
/// `shar file...`
///
/// Archives one or more files into a shell script that can be executed to
/// recreate the original files.
///
/// @ingroup utility
/// @note Requires `gres` utility for extraction.
/// @see gres.cpp
/// @see https://en.wikipedia.org/wiki/Shar
/// @see https://pubs.opengroup.org/onlinepubs/9699919799/utilities/shar.html
///
/// @author XINIM Project (Modernized from original UNIX implementation)
/// @version 1.0
/// @date 2024-01-17
/// @copyright Copyright (c) 2024, The XINIM Project. All rights reserved.

#include "blocksiz.hpp" // Defines BLOCK_SIZE

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept> // For std::ios_base::failure

namespace fs = std::filesystem;

/// Size of the internal I/O buffer used when copying data.
inline constexpr std::size_t IO_SIZE = 10 * BLOCK_SIZE;

/// @brief Emits shell archive commands for a single file.
///
/// This function generates a sequence of `echo` and `gres` commands that,
/// when executed in a shell, will recreate the specified file.
///
/// @param path Path to the file that will be archived.
/// @return `void`; writes shell reconstruction commands to standard output.
/// @throws `std::ios_base::failure` if there's an I/O error during file processing.
static void emit_archive(const fs::path &path);

/// @brief Encodes an input stream for inclusion in the archive.
///
/// Reads the content of a file line by line and prefixes each line with 'X'
/// for later extraction by `gres`.
///
/// @param in Stream from which to read the file contents.
/// @return `void`; encoded data lines are written to standard output.
/// @throws `std::ios_base::failure` if a read error occurs on the input stream.
static void encode_stream(std::istream &in);

/**
 * @brief Entry point for the `shar` utility.
 *
 * This function processes the command-line arguments to archive one or more
 * files. It iterates through the provided file paths, calling `emit_archive`
 * for each to generate the necessary shell commands.
 *
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return `0` on success, `1` if invalid arguments or file errors occur.
 * @ingroup utility
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: shar file...\n";
        return 1; // Indicate error for invalid arguments.
    }

    for (int i = 1; i < argc; ++i) {
        try {
            emit_archive(argv[i]);
        } catch (const std::ios_base::failure& e) {
            std::cerr << "shar: I/O error processing " << argv[i] << ": " << e.what() << "\n";
            return 1; // Indicate error.
        } catch (const std::exception& e) {
            std::cerr << "shar: unexpected error processing " << argv[i] << ": " << e.what() << "\n";
            return 1; // Indicate error.
        }
    }
    return 0; // Indicate success.
}

void emit_archive(const fs::path &path) {
    std::ifstream in{path, std::ios::binary}; // RAII for file stream.
    if (!in) {
        // No need to throw here, as main() handles the return.
        std::cerr << "shar: cannot open " << path << "\n";
        return;
    }

    // Emit commands to recreate the file.
    std::cout << "echo x - " << path.string() << "\n";
    std::cout << "gres '^X' '' > " << path.string() << " << '/'\n"; // `gres` is used to strip 'X' prefix.
    encode_stream(in); // Encode and output file content.
    std::cout << "/\n"; // Delimiter for `gres`.
}

void encode_stream(std::istream &in) {
    std::string line;
    line.reserve(IO_SIZE); // Pre-allocate buffer to reduce reallocations.
    while (std::getline(in, line)) {
        std::cout << 'X' << line << '\n'; // Prefix each line with 'X'.
    }
    if (in.bad()) {
        // Check for actual read errors, not just EOF.
        throw std::ios_base::failure("read error");
    }
}