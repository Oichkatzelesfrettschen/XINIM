/**
 * @file test_net_driver_unpriv_id.cpp
 * @brief Verify node identifier persistence in unprivileged directories.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

int main() {
    const char *tmp = std::getenv("TMPDIR");
    if (!tmp)
        tmp = "/tmp";
    std::filesystem::path dir = std::filesystem::path{tmp} / "xinim_unpriv";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    setenv("XDG_STATE_HOME", dir.c_str(), 1);
    setuid(65534); // drop privileges

    net::init({0, 16010});
    const auto first = net::local_node();
    assert(first != 0);
    net::shutdown();

    net::init({0, 16010});
    const auto second = net::local_node();
    assert(second == first);
    net::shutdown();

    assert(std::filesystem::exists(dir / "xinim" / "node_id"));
    std::filesystem::remove_all(dir);
    return 0;
}
