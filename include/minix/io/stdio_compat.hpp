#pragma once

/**
 * @file stdio_compat.hpp
 * @brief C stdio compatibility shims using Streams.
 */

#include "file_operations.hpp"
#include "standard_streams.hpp"
#include <cstdio>

namespace minix::io::compat {

/// Associate a FILE handle with a Stream.
void register_file_stream(FILE *file, Stream *stream);
/// Retrieve the Stream linked to a FILE handle.
Stream *get_stream(FILE *file);

extern "C" {
/// fopen replacement backed by Streams.
FILE *fopen_compat(const char *path, const char *mode);
/// fclose replacement for Stream-backed handles.
int fclose_compat(FILE *fp);
/// fread replacement using Stream::read.
size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp);
/// fwrite replacement using Stream::write.
size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp);
/// fprintf replacement using Streams.
int fprintf_compat(FILE *fp, const char *format, ...);
}

} // namespace minix::io::compat
