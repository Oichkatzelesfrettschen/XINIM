#include "../include/lib.hpp" // C++23 header

#include <chrono>
#include <csignal>
#include <mutex>
#include <thread>

/// \brief Schedule a SIGALRM delivery after \p sec seconds.
///
/// Cancels any existing alarm and returns the remaining time of the
/// previous timer in whole seconds. The implementation leverages
/// `std::jthread` and `std::chrono` facilities to provide a modern,
/// thread-based timer that checks for cancellation via
/// `std::stop_token`.
///
/// \param sec Number of seconds to wait before raising SIGALRM. A value
/// of zero cancels any pending alarm.
/// \return Remaining time of the previous alarm in seconds, or zero if
/// none was pending.
extern "C" int xinim_alarm(unsigned int sec) {
    using namespace std::chrono;

    static std::mutex alarm_mutex;             //!< Guards shared alarm state.
    static std::jthread worker;                //!< Worker thread handling the timer.
    static steady_clock::time_point deadline = //!< Time when the current alarm expires.
        steady_clock::time_point::min();

    std::lock_guard lock{alarm_mutex};

    int remaining{0};
    if (deadline != steady_clock::time_point::min()) {
        const auto now = steady_clock::now();
        if (deadline > now) {
            remaining = static_cast<int>(duration_cast<seconds>(deadline - now).count());
        }
        worker.request_stop();
        if (worker.joinable()) {
            worker.join();
        }
        deadline = steady_clock::time_point::min();
    }

    if (sec == 0U) {
        return remaining;
    }

    deadline = steady_clock::now() + seconds{sec};
    worker = std::jthread(
        [](std::stop_token st, seconds duration) {
            const auto start = steady_clock::now();
            while (!st.stop_requested()) {
                if (steady_clock::now() - start >= duration) {
                    std::raise(SIGALRM);
                    break;
                }
                std::this_thread::sleep_for(milliseconds{10});
            }
        },
        seconds{sec});

    return remaining;
}
