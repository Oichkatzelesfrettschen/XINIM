/**
 * @file pipe.cpp
 * @brief Pipe implementation for inter-process communication
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#include "pipe.hpp"
#include "pcb.hpp"
#include "scheduler.hpp"
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace xinim::kernel {

// ============================================================================
// Pipe Write Operation
// ============================================================================

/**
 * @brief Write data to pipe
 *
 * Writes data to ring buffer. Blocks if pipe is full.
 * Wakes blocked readers after writing.
 *
 * @param data Source buffer (kernel space)
 * @param len Number of bytes to write
 * @return Bytes written or negative error
 */
ssize_t Pipe::write(const void* data, size_t len) {
    // Check if read end is closed
    if (!read_end_open) {
        return -EPIPE;  // Broken pipe
    }

    // Week 9: Simplified - write all or nothing (no partial writes)
    // Week 10: Will support partial writes for non-blocking I/O

    // Wait until enough space is available
    while (available() < len && read_end_open) {
        // Block current process
        ProcessControlBlock* current = get_current_process();
        if (!current) return -ESRCH;

        current->state = ProcessState::BLOCKED;
        current->blocked_on = BlockReason::IO;

        // Add to writers list
        current->next = writers_head;
        writers_head = current;

        // Yield to scheduler
        schedule();

        // When we wake up, check if read end was closed
        if (!read_end_open) {
            return -EPIPE;
        }
    }

    // Write data to ring buffer
    size_t bytes_written = 0;
    const char* src = static_cast<const char*>(data);

    while (bytes_written < len && count < PIPE_BUF) {
        buffer[write_pos] = src[bytes_written];
        write_pos = (write_pos + 1) % PIPE_BUF;
        count++;
        bytes_written++;
    }

    // Wake up all blocked readers
    while (readers_head) {
        ProcessControlBlock* reader = readers_head;
        readers_head = reader->next;
        reader->state = ProcessState::READY;
        reader->blocked_on = BlockReason::NONE;
        reader->next = nullptr;
    }

    return (ssize_t)bytes_written;
}

// ============================================================================
// Pipe Read Operation
// ============================================================================

/**
 * @brief Read data from pipe
 *
 * Reads data from ring buffer. Blocks if pipe is empty and write end is open.
 * Returns 0 (EOF) if pipe is empty and write end is closed.
 *
 * @param data Destination buffer (kernel space)
 * @param len Maximum bytes to read
 * @return Bytes read (0 = EOF) or negative error
 */
ssize_t Pipe::read(void* data, size_t len) {
    // Check for EOF: write end closed and pipe is empty
    if (!write_end_open && count == 0) {
        return 0;  // EOF
    }

    // Wait until data is available or write end closes
    while (count == 0 && write_end_open) {
        // Block current process
        ProcessControlBlock* current = get_current_process();
        if (!current) return -ESRCH;

        current->state = ProcessState::BLOCKED;
        current->blocked_on = BlockReason::IO;

        // Add to readers list
        current->next = readers_head;
        readers_head = current;

        // Yield to scheduler
        schedule();

        // When we wake up, check for EOF again
        if (!write_end_open && count == 0) {
            return 0;  // EOF
        }
    }

    // Read data from ring buffer (up to len bytes or all available)
    size_t bytes_read = 0;
    char* dest = static_cast<char*>(data);
    size_t to_read = std::min(len, count);

    while (bytes_read < to_read) {
        dest[bytes_read] = buffer[read_pos];
        read_pos = (read_pos + 1) % PIPE_BUF;
        count--;
        bytes_read++;
    }

    // Wake up all blocked writers
    while (writers_head) {
        ProcessControlBlock* writer = writers_head;
        writers_head = writer->next;
        writer->state = ProcessState::READY;
        writer->blocked_on = BlockReason::NONE;
        writer->next = nullptr;
    }

    return (ssize_t)bytes_read;
}

// ============================================================================
// Pipe Close Operations
// ============================================================================

/**
 * @brief Close read end of pipe
 *
 * Marks read end as closed. Wakes all blocked writers with EPIPE error.
 */
void Pipe::close_read_end() {
    read_end_open = false;

    // Wake up all blocked writers (they'll get EPIPE)
    while (writers_head) {
        ProcessControlBlock* writer = writers_head;
        writers_head = writer->next;
        writer->state = ProcessState::READY;
        writer->blocked_on = BlockReason::NONE;
        writer->next = nullptr;
    }
}

/**
 * @brief Close write end of pipe
 *
 * Marks write end as closed. Wakes all blocked readers (they'll see EOF).
 */
void Pipe::close_write_end() {
    write_end_open = false;

    // Wake up all blocked readers (they'll see EOF)
    while (readers_head) {
        ProcessControlBlock* reader = readers_head;
        readers_head = reader->next;
        reader->state = ProcessState::READY;
        reader->blocked_on = BlockReason::NONE;
        reader->next = nullptr;
    }
}

} // namespace xinim::kernel
