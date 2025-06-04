#pragma once

#include "file_operations.hpp"
#include "standard_streams.hpp"
#include <cstdio>

namespace minix::io::compat {

void register_file_stream(FILE *file, Stream *stream);
Stream *get_stream(FILE *file);

extern "C" {
FILE *fopen_compat(const char *path, const char *mode);
int fclose_compat(FILE *fp);
size_t fread_compat(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t fwrite_compat(const void *ptr, size_t size, size_t nmemb, FILE *fp);
int fprintf_compat(FILE *fp, const char *format, ...);
}

} // namespace minix::io::compat
