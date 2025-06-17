/**
 * @file test_service_restart_remote.cpp
 * @brief Verify crash propagation across nodes.
 */

#include "../kernel/lattice_ipc.hpp"
#include "../kernel/net_driver.hpp"
#include "../kernel/schedule.hpp"
#include "../kernel/service.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>

using namespace lattice;
using namespace svc;
using namespace sched;

static constexpr net::node_t PARENT_NODE = 0;
static constexpr net::node_t CHILD_NODE = 1;
static constexpr uint16_t PARENT_PORT = 16000;
static constexpr uint16_t CHILD_PORT = 16001;

static int child_proc() {
    net::init({CHILD_NODE, CHILD_PORT});
    net::add_remote(PARENT_NODE, "127.0.0.1", PARENT_PORT);
    g_graph = Graph{};
    lattice_connect(2, 1, PARENT_NODE);

    service_manager.register_service(2, {}, 1, net::local_node());
    scheduler.enqueue(2);
    scheduler.crash(2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    net::shutdown();
    return 0;
}

static int parent_proc(pid_t child) {
    net::init({PARENT_NODE, PARENT_PORT});
    net::add_remote(CHILD_NODE, "127.0.0.1", CHILD_PORT);
    g_graph = Graph{};
    lattice_connect(1, 2, CHILD_NODE);

    service_manager.register_service(2, {}, 1, CHILD_NODE);

    for (int i = 0; i < 20; ++i) {
        poll_network();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int status = 0;
    waitpid(child, &status, 0);
    net::shutdown();

    assert(service_manager.contract(2).restarts == 1);
    return status;
}

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        return child_proc();
    }
    return parent_proc(pid);
}
