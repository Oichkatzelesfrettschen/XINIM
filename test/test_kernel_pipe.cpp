#include "src/kernel/pipe.hpp"
#include "src/kernel/pcb.hpp"
#include "src/kernel/scheduler.hpp"
#include "src/kernel/signal.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <print>

namespace {

using xinim::kernel::BlockReason;
using xinim::kernel::Pipe;
using xinim::kernel::ProcessControlBlock;
using xinim::kernel::ProcessState;

ProcessControlBlock* g_current_process = nullptr;
ProcessControlBlock* g_last_signaled_process = nullptr;
int g_last_signal = 0;

bool expect(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::println(std::cerr, "FAIL: {}", message);
        ++failures;
        return false;
    }
    return true;
}

Pipe make_pipe() {
    Pipe pipe{};
    pipe.read_pos = 0;
    pipe.write_pos = 0;
    pipe.count = 0;
    pipe.read_end_open = true;
    pipe.write_end_open = true;
    pipe.readers_head = nullptr;
    pipe.writers_head = nullptr;
    return pipe;
}

void reset_stub_state() {
    g_current_process = nullptr;
    g_last_signaled_process = nullptr;
    g_last_signal = 0;
}

} // namespace

namespace xinim::kernel {

ProcessControlBlock* get_current_process() {
    return ::g_current_process;
}

void schedule() {}

int send_signal(ProcessControlBlock* pcb, int signum) {
    ::g_last_signaled_process = pcb;
    ::g_last_signal = signum;
    return 0;
}

} // namespace xinim::kernel

int main() {
    int failures = 0;
    constexpr int expected_sigpipe = 13;

    {
        Pipe pipe = make_pipe();
        const char payload[] = "xinim";
        char out[sizeof(payload)]{};

        expect(pipe.write(payload, sizeof(payload)) == static_cast<ssize_t>(sizeof(payload)),
               "write should store the full payload when space is available",
               failures);
        expect(pipe.count == sizeof(payload), "pipe count should track written bytes", failures);
        expect(pipe.read(out, sizeof(out)) == static_cast<ssize_t>(sizeof(out)),
               "read should return the written payload length",
               failures);
        expect(std::memcmp(out, payload, sizeof(payload)) == 0,
               "read should preserve payload contents",
               failures);
        expect(pipe.is_empty(), "pipe should be empty after reading back all bytes", failures);
    }

    {
        reset_stub_state();
        Pipe pipe = make_pipe();
        ProcessControlBlock writer{};
        pipe.close_read_end();
        g_current_process = &writer;

        expect(pipe.write("x", 1) == -EPIPE, "write should fail with EPIPE when readers are closed",
               failures);
        expect(g_last_signaled_process == &writer,
               "write with closed readers should signal the current writer",
               failures);
        expect(g_last_signal == expected_sigpipe,
               "write with closed readers should send SIGPIPE",
               failures);
    }

    {
        Pipe pipe = make_pipe();
        ProcessControlBlock reader{};
        reader.state = ProcessState::BLOCKED;
        reader.blocked_on = BlockReason::IO;
        pipe.readers_head = &reader;

        pipe.close_write_end();
        expect(pipe.readers_head == nullptr, "close_write_end should wake blocked readers", failures);
        expect(reader.state == ProcessState::READY, "reader should move to READY on write-end close",
               failures);
        expect(reader.blocked_on == BlockReason::NONE,
               "reader block reason should clear on write-end close",
               failures);
        expect(pipe.read(nullptr, 0) == 0, "empty closed pipe should read as EOF", failures);
    }

    {
        Pipe pipe = make_pipe();
        ProcessControlBlock writer{};
        writer.state = ProcessState::BLOCKED;
        writer.blocked_on = BlockReason::IO;
        pipe.writers_head = &writer;

        pipe.close_read_end();
        expect(pipe.writers_head == nullptr, "close_read_end should wake blocked writers", failures);
        expect(writer.state == ProcessState::READY, "writer should move to READY on read-end close",
               failures);
        expect(writer.blocked_on == BlockReason::NONE,
               "writer block reason should clear on read-end close",
               failures);
    }

    {
        reset_stub_state();
        Pipe pipe = make_pipe();
        pipe.count = PIPE_BUF - 1;
        pipe.write_pos = PIPE_BUF - 1;

        expect(pipe.write("xy", 2) == -ESRCH,
               "write should fail with ESRCH if blocking is required and no current process exists",
               failures);
    }

    {
        reset_stub_state();
        Pipe pipe = make_pipe();
        expect(pipe.read(nullptr, 1) == -ESRCH,
               "read should fail with ESRCH if blocking is required and no current process exists",
               failures);
    }

    if (failures != 0) {
        std::println(std::cerr, "{} kernel pipe test(s) failed.", failures);
        return EXIT_FAILURE;
    }

    std::println(std::cout, "ALL kernel pipe tests passed.");
    return EXIT_SUCCESS;
}
