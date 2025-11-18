/**
 * @file pipe.hpp
 * @brief Pipe implementation for inter-process communication
 *
 * Provides unidirectional byte stream between two file descriptors.
 *
 * @ingroup kernel
 * @author XINIM Development Team
 * @date November 2025
 */

#ifndef XINIM_KERNEL_PIPE_HPP
#define XINIM_KERNEL_PIPE_HPP

#include <cstdint>
#include <cstddef>
#include <sys/types.h>

namespace xinim::kernel {

// Forward declaration
struct ProcessControlBlock;

/**
 * @brief POSIX pipe buffer size
 *
 * Writes of <= PIPE_BUF bytes are guaranteed to be atomic.
 */
constexpr size_t PIPE_BUF = 4096;

/**
 * @brief Pipe structure (shared between read and write FDs)
 *
 * Implements a ring buffer for unidirectional communication between processes.
 *
 * Properties:
 * - Unidirectional (write end → read end)
 * - Blocking I/O (read blocks if empty, write blocks if full)
 * - Atomic writes < PIPE_BUF
 * - EOF when write end closed and buffer empty
 */
struct Pipe {
    // ========================================
    // Ring Buffer
    // ========================================

    char buffer[PIPE_BUF];       ///< Data storage
    size_t read_pos;             ///< Read position in buffer
    size_t write_pos;            ///< Write position in buffer
    size_t count;                ///< Number of bytes currently in pipe

    // ========================================
    // End Status
    // ========================================

    bool read_end_open;          ///< Is read end (FD) still open?
    bool write_end_open;         ///< Is write end (FD) still open?

    // ========================================
    // Blocking Lists
    // ========================================

    ProcessControlBlock* readers_head;  ///< Linked list of blocked readers
    ProcessControlBlock* writers_head;  ///< Linked list of blocked writers

    // ========================================
    // Operations
    // ========================================

    /**
     * @brief Write data to pipe
     *
     * Writes up to len bytes from data into pipe buffer.
     * Blocks if pipe is full and write end is open.
     *
     * @param data Source buffer (kernel space)
     * @param len Number of bytes to write
     * @return Number of bytes written, or negative error
     *         -EPIPE: Write end closed (broken pipe)
     */
    ssize_t write(const void* data, size_t len);

    /**
     * @brief Read data from pipe
     *
     * Reads up to len bytes from pipe into data buffer.
     * Blocks if pipe is empty and write end is open.
     *
     * @param data Destination buffer (kernel space)
     * @param len Maximum bytes to read
     * @return Number of bytes read (0 = EOF), or negative error
     */
    ssize_t read(void* data, size_t len);

    /**
     * @brief Close read end of pipe
     *
     * Marks read end as closed. Wakes blocked writers with EPIPE.
     */
    void close_read_end();

    /**
     * @brief Close write end of pipe
     *
     * Marks write end as closed. Wakes blocked readers (they'll see EOF).
     */
    void close_write_end();

    /**
     * @brief Check if pipe buffer is full
     */
    bool is_full() const { return count == PIPE_BUF; }

    /**
     * @brief Check if pipe buffer is empty
     */
    bool is_empty() const { return count == 0; }

    /**
     * @brief Get available space for writing
     */
    size_t available() const { return PIPE_BUF - count; }
};

} // namespace xinim::kernel

#endif /* XINIM_KERNEL_PIPE_HPP */
