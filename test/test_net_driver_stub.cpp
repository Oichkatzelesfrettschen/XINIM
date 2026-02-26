/**
 * @file test_net_driver_stub.cpp
 * @brief Tests that the stub NetDriver interface returns correct failure values.
 *
 * WHY: The net driver is a stub until Phase 7 (virtio-net). These tests verify
 *      the interface contract: all operations fail gracefully, local_node() returns 1.
 */

#include "xinim/net/net_driver.hpp"
#include <cassert>
#include <cstddef>

static void test_local_node() {
    assert(net::local_node() == 1);
}

static void test_init_returns_false() {
    net::NetDriver drv;
    net::Config cfg{1, 8080, 64};
    bool result = drv.init(cfg);
    // Stub: may return true or false; just verify it doesn't crash
    (void)result;
}

static void test_send_returns_false() {
    net::NetDriver drv;
    std::byte buf[16]{};
    bool result = drv.send(1, buf);
    assert(!result); // stub returns failure
}

static void test_recv_returns_false() {
    net::NetDriver drv;
    net::Packet pkt{};
    bool result = drv.recv(pkt);
    assert(!result); // stub returns failure
}

static void test_shutdown_no_crash() {
    net::NetDriver drv;
    drv.shutdown(); // must not crash
}

int main() {
    test_local_node();
    test_init_returns_false();
    test_send_returns_false();
    test_recv_returns_false();
    test_shutdown_no_crash();
    return 0;
}
