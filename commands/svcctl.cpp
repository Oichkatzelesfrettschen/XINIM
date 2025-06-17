#include "svcctl.hpp"
#include "../kernel/lattice_ipc.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

using namespace lattice;

namespace svcctl {

/** Ensure a connection exists between the client and manager. */
static void ensure_connection() {
    lattice_connect(CLIENT_PID, MANAGER_PID);
    lattice_connect(MANAGER_PID, CLIENT_PID);
}

/** Send a simple control message carrying @p pid. */
static void send_simple(Message type, xinim::pid_t pid) {
    message msg{};
    msg.m_type = static_cast<int>(type);
    msg.m1_i1() = static_cast<int>(pid);
    lattice_send(CLIENT_PID, MANAGER_PID, msg);
}

/** Receive a message into @p out using blocking semantics. */
static void recv_blocking(message &out) {
    while (lattice_recv(CLIENT_PID, &out) != xinim::OK) {
    }
}

int run(std::span<char *> args) {
    if (args.size() < 2) {
        std::puts("usage: svcctl <list|start|stop|restart> [pid]");
        return 1;
    }

    ensure_connection();

    std::string_view sub{args[1]};
    if (sub == "list") {
        message req{};
        req.m_type = static_cast<int>(Message::LIST);
        lattice_send(CLIENT_PID, MANAGER_PID, req);
#ifndef SVCCTL_NO_WAIT
        for (;;) {
            message resp{};
            recv_blocking(resp);
            if (resp.m_type == static_cast<int>(Message::END)) {
                break;
            }
            if (resp.m_type == static_cast<int>(Message::LIST_RESPONSE)) {
                std::printf("%d %s\n", resp.m1_i1(), resp.m1_i2() ? "running" : "stopped");
            }
        }
#endif
        return 0;
    }

    if (args.size() < 3) {
        std::puts("pid required");
        return 1;
    }
    xinim::pid_t pid = static_cast<xinim::pid_t>(std::atoi(args[2]));

    if (sub == "start") {
        send_simple(Message::START, pid);
    } else if (sub == "stop") {
        send_simple(Message::STOP, pid);
    } else if (sub == "restart") {
        send_simple(Message::RESTART, pid);
    } else {
        std::puts("unknown subcommand");
        return 1;
    }

#ifndef SVCCTL_NO_WAIT
    message ack{};
    recv_blocking(ack);
#endif
    return 0;
}

} // namespace svcctl

#ifndef SVCCTL_NO_MAIN
int main(int argc, char **argv) { return svcctl::run({argv, static_cast<size_t>(argc)}); }
#endif
