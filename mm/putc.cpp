/**
 * @file putc.cpp
 * @brief RAII-based buffered output for the memory manager.
 *
 * Characters are accumulated in a local buffer and forwarded to the TTY task
 * when the buffer is full or a newline is encountered. The buffer is flushed
 * automatically on destruction to ensure no output is lost.
 */

#include "../h/com.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"

#include <array>
#include <cstddef>

namespace {

/**
 * @brief RAII-managed output buffer.
 */
class OutputBuffer {
  public:
    /// Adds a character to the buffer, flushing as required.
    void put(char c) {
        buffer_[count_++] = c;
        if (count_ == buffer_.size() || c == '\n') {
            flush();
        }
    }

    /// Flushes buffered characters to the TTY task.
    void flush() {
        if (count_ == 0) {
            return;
        }
        message msg{};
        msg.m_type = TTY_WRITE;
        proc_nr(msg) = 0;
        tty_line(msg) = 0;
        address(msg) = buffer_.data();
        count(msg) = static_cast<int>(count_);
        sendrec(TTY, &msg);
        count_ = 0;
    }

    /// Destructor ensures pending output is transmitted.
    ~OutputBuffer() { flush(); }

  private:
    static constexpr std::size_t BUF_SIZE = 100; ///< Print buffer size.
    std::array<char, BUF_SIZE> buffer_{};        ///< Internal character buffer.
    std::size_t count_ = 0;                      ///< Characters currently stored.
};

OutputBuffer g_output; ///< Global buffer instance.

} // namespace

/**
 * @brief Write a character to the memory manager's buffered output.
 *
 * @param c Character to emit.
 */
void putc(char c) { g_output.put(c); }
