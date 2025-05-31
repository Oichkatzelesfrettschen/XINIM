#include "buf.hpp"

// Global buffer cache structures
buf buf[NR_BUFS];

buf *buf_hash[NR_BUF_HASH]; // buffer hash table
buf *front = nullptr;       // least recently used free block
buf *rear = nullptr;        // most recently used free block
int bufs_in_use = 0;        // number of buffers currently in use
