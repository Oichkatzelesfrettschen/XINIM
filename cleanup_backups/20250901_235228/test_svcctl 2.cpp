/**
 * @file test_svcctl.cpp
 * @brief Unit tests validating the svcctl command.
 */

#include "../commands/svcctl.hpp"
#include "../kernel/lattice_ipc.hpp"
#include <cassert>
#include <unordered_map>

using namespace lattice;
using svcctl::Message;

/** Simple threaded mock service manager responding over lattice IPC. */
class MockServiceManager {
  public:
    std::unordered_map<xinim::pid_t, bool> services; ///< Running state per service

    /** Process a single pending control message. */
    void process_once() {
        message msg{};
        if (lattice_recv(svcctl::MANAGER_PID, &msg, IpcFlags::NONBLOCK) != xinim::OK) {
            return;
        }
        switch (static_cast<Message>(msg.m_type)) {
        case Message::LIST:
            for (auto &[pid, active] : services) {
                message out{};
                out.m_type = static_cast<int>(Message::LIST_RESPONSE);
                out.m1_i1() = static_cast<int>(pid);
                out.m1_i2() = active ? 1 : 0;
                lattice_send(svcctl::MANAGER_PID, svcctl::CLIENT_PID, out);
            }
            {
                message end{};
                end.m_type = static_cast<int>(Message::END);
                lattice_send(svcctl::MANAGER_PID, svcctl::CLIENT_PID, end);
            }
            break;
        case Message::START:
            services[static_cast<xinim::pid_t>(msg.m1_i1())] = true;
            send_ack();
            break;
        case Message::STOP:
            services[static_cast<xinim::pid_t>(msg.m1_i1())] = false;
            send_ack();
            break;
        case Message::RESTART:
            services[static_cast<xinim::pid_t>(msg.m1_i1())] = true;
            send_ack();
            break;
        default:
            break;
        }
    }

  private:
    /** Helper to send a generic acknowledgement. */
    void send_ack() {
        message ack{};
        ack.m_type = static_cast<int>(Message::ACK);
        lattice_send(svcctl::MANAGER_PID, svcctl::CLIENT_PID, ack);
    }
};

int main() {
    g_graph = Graph{};
    lattice_connect(svcctl::CLIENT_PID, svcctl::MANAGER_PID);
    lattice_connect(svcctl::MANAGER_PID, svcctl::CLIENT_PID);

    MockServiceManager mgr;
    mgr.services[10] = false;

    char *start_args[] = {const_cast<char *>("svcctl"), const_cast<char *>("start"),
                          const_cast<char *>("10")};
    assert(svcctl::run({start_args, 3}) == 0);
    mgr.process_once();
    assert(mgr.services[10]);

    char *stop_args[] = {const_cast<char *>("svcctl"), const_cast<char *>("stop"),
                         const_cast<char *>("10")};
    assert(svcctl::run({stop_args, 3}) == 0);
    mgr.process_once();
    assert(!mgr.services[10]);

    char *restart_args[] = {const_cast<char *>("svcctl"), const_cast<char *>("restart"),
                            const_cast<char *>("10")};
    assert(svcctl::run({restart_args, 3}) == 0);
    mgr.process_once();
    assert(mgr.services[10]);

    char *list_args[] = {const_cast<char *>("svcctl"), const_cast<char *>("list")};
    assert(svcctl::run({list_args, 2}) == 0);
    for (int i = 0; i < 4; ++i) {
        mgr.process_once();
    }

    return 0;
}
