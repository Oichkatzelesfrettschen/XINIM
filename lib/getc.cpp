#include "../include/stdio.hpp"
#include <ext/stdio_filebuf.h>
#include <istream>
#include <ranges>

/**
 * @file
 * @brief Character retrieval using modern C++ streams and ranges.
 */

/**
 * @brief Retrieve a single character from the given stream.
 *
 * The underlying file descriptor is wrapped in a GNU `stdio_filebuf` to expose
 * a standard C++ `std::istream` interface.  A `std::ranges::istream_view` then
 * pulls exactly one character from the stream.  On end-of-file or error the
 * corresponding flags in @p iop are set and ::STDIO_EOF is returned.
 *
 * @param iop Pointer to the legacy FILE structure managing the descriptor.
 * @return Next character as an unsigned char cast to int, or ::STDIO_EOF on
 *         failure.
 */
int getc(FILE *iop) {
    if (iop == nullptr || testflag(iop, (_EOF | _ERR))) {
        return STDIO_EOF;
    }

    __gnu_cxx::stdio_filebuf<char> buf{iop->_fd, std::ios::in};
    std::istream stream{&buf};

    auto view = std::ranges::istream_view<char>(stream);
    auto it = view.begin();

    if (it == view.end()) {
        if (stream.eof()) {
            iop->_flags |= _EOF;
        } else {
            iop->_flags |= _ERR;
        }
        return STDIO_EOF;
    }

    return static_cast<unsigned char>(*it);
}
