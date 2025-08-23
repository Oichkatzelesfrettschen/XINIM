/// \file shar.cpp
/// \brief Generate a shell archive from one or more files.
///
/// This utility reproduces the classic `shar` program using modern
/// C++23 facilities.  Each input file is emitted as a sequence of shell
/// commands that reconstruct the file when executed.  Lines are prefixed
/// with the character `X` so that a simple `gres` command can strip the
/// prefix during extraction.

#include "blocksiz.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

/// Size of the internal I/O buffer used when copying data.
inline constexpr std::size_t IO_SIZE = 10 * BLOCK_SIZE;

/// \brief Emit archive commands for a single file.
/// \param path Path to the file that will be archived.
static void emit_archive(const fs::path &path);

/// \brief Encode an input stream for inclusion in the archive.
/// \param in Stream from which to read the file contents.
static void encode_stream(std::istream &in);

/// \brief Program entry point.
/// \param argc Number of command line arguments.
/// \param argv Argument vector.
/// \return Zero on success, non–zero otherwise.
/**
 * @brief Entry point for the shar utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: shar file...\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        emit_archive(argv[i]);
    }
    return 0;
}

void emit_archive(const fs::path &path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        std::cerr << "shar: cannot open " << path << "\n";
        return;
    }

    std::cout << "echo x - " << path.string() << "\n";
    std::cout << "gres '^X' '' > " << path.string() << " << '/'\n";
    encode_stream(in);
    std::cout << "/\n";
}

void encode_stream(std::istream &in) {
    std::string line;
    line.reserve(IO_SIZE);
    while (std::getline(in, line)) {
        std::cout << 'X' << line << '\n';
    }
    if (in.bad()) {
        throw std::ios_base::failure("read error");
    }
}
