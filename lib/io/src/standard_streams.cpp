#include "../../../include/minix/io/standard_streams.hpp"
#include "../../../include/minix/io/file_stream.hpp"    // For FileStream concrete type
#include <array>
#include <memory>    // For std::unique_ptr, std::make_unique
#include <algorithm> // For std::find_if
#include <stdexcept> // For std::out_of_range (though not used in final version)
#include <fcntl.h>   // For O_RDONLY, O_WRONLY (used by initialize_standard_streams)

namespace minix::io {

// Global table of stream pointers
// Each element is a unique_ptr to a Stream object.
// This table owns the stream objects.
static std::array<StreamPtr, max_open_streams> global_stream_table;

void initialize_standard_streams() {
    // stdin - fd 0
    // FileStream constructor takes: syscall::fd_t fd, int open_flags, BufferMode mode
    global_stream_table[0] = std::make_unique<FileStream>(
        syscall::fd_t{0},
        O_RDONLY, // Standard open flags for stdin (read-only)
        Stream::BufferMode::line // Typically line-buffered
    );

    // stdout - fd 1
    global_stream_table[1] = std::make_unique<FileStream>(
        syscall::fd_t{1},
        O_WRONLY, // Standard open flags for stdout (write-only)
        Stream::BufferMode::line // Typically line-buffered
    );

    // stderr - fd 2
    global_stream_table[2] = std::make_unique<FileStream>(
        syscall::fd_t{2},
        O_WRONLY, // Standard open flags for stderr (write-only)
        Stream::BufferMode::none // Typically unbuffered
    );

    // Initialize remaining slots in the table to nullptr, indicating they are free
    for (size_t i = 3; i < max_open_streams; ++i) {
        global_stream_table[i] = nullptr;
    }
}

Stream& get_standard_input() noexcept {
    // Assumes initialize_standard_streams() has been called.
    // If global_stream_table[0] is somehow null after init, this would dereference null.
    // In a robust system, one might add a check or ensure init order.
    return *global_stream_table[0];
}

Stream& get_standard_output() noexcept {
    return *global_stream_table[1];
}

Stream& get_standard_error() noexcept {
    return *global_stream_table[2];
}

Result<Stream*> open_new_stream(const char* path, int posix_flags, int mode_permissions, BufferMode buff_mode) {
    // Find an empty slot in the global table (typically start search after stderr, index 3)
    auto it = std::find_if(global_stream_table.begin() + 3, global_stream_table.end(),
                           [](const StreamPtr& s_ptr){ return s_ptr == nullptr; });

    if (it == global_stream_table.end()) {
        return std::unexpected(make_error_code(IOError::resource_exhausted)); // No free slots
    }

    // Use the FileStream static factory 'open' method
    Result<StreamPtr> new_file_stream_res = FileStream::open(path, posix_flags, mode_permissions, buff_mode);

    if (!new_file_stream_res) {
        return std::unexpected(new_file_stream_res.error()); // Factory method failed (e.g., syscall::open failed)
    }

    // Move the successfully created StreamPtr (owning a FileStream) into the table slot
    *it = std::move(new_file_stream_res.value());

    // Return a raw pointer to the Stream object now managed by the table
    return it->get();
}

Result<void> close_managed_stream(Stream* stream_raw_ptr) {
   if (!stream_raw_ptr) {
       return std::unexpected(make_error_code(IOError::invalid_argument));
   }
   auto it = std::find_if(global_stream_table.begin(), global_stream_table.end(),
                          [&](const StreamPtr& s_ptr){ return s_ptr.get() == stream_raw_ptr; });

   if (it != global_stream_table.end()) {
       if (*it) { // Check if the unique_ptr is not null
           Result<void> close_res = (*it)->close(); // Explicitly close the stream
           // Even if close() returns an error, we remove it from the table.
           // The unique_ptr's destructor will handle resource cleanup if close() didn't.
           it->reset(); // Release and delete the Stream object, making the slot available.
           if (!close_res) return close_res; // Propagate close error if any
       } else {
            // This case (iterator points to a slot that is already nullptr) should ideally not happen
            // if stream_raw_ptr was valid and came from open_new_stream.
            // However, if it does, the slot is already "free".
       }
       return {}; // Success
   }
   // If the raw pointer is not found in the table, it's an invalid argument or double close.
   return std::unexpected(make_error_code(IOError::invalid_argument));
}

void flush_all_open_streams() noexcept {
    // global_stream_table is static in this CPP file, so it's accessible here.
    for (const auto& stream_ptr : global_stream_table) {
        if (stream_ptr) { // Check if unique_ptr is not null
            // Check stream state before attempting to flush.
            // needs_flush() is a good preliminary check for writable streams with data.
            // For readable streams, flush might not be meaningful or could be harmful.
            // The Stream::flush() itself should handle if it's a no-op for its type/state.
            // Consider also stream_ptr->is_open() if flush() on a non-open stream is problematic.
            if (stream_ptr->is_open() && stream_ptr->is_writable()) {
                 // Call needs_flush() only if it's certain that flush on a non-dirty stream is cheap.
                 // Otherwise, just calling flush() and letting it decide is also fine.
                if (stream_ptr->needs_flush()) { // needs_flush checks for dirty write buffer
                    [[maybe_unused]] auto result = stream_ptr->flush();
                    // In a cleanup routine, errors from individual flushes are typically ignored,
                    // as the program is terminating. Logging could be an option if available.
                }
            }
        }
    }
}

} // namespace minix::io
