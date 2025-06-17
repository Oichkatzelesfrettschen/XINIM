/**
 * @file test_net_driver_id.cpp
 * @brief Ensure automatic node identifier detection.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <string_view>
#include <unistd.h>

namespace {

/**
 * @brief Replicate the driver's detection logic to obtain the expected
 *        identifier.
 */
[[nodiscard]] net::node_t compute_expected() {
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
            if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
                continue;
            }
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
                auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
                std::size_t value = 0;
                for (int i = 0; i < ll->sll_halen; ++i) {
                    value = value * 131 + ll->sll_addr[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(cur->ifa_addr);
                std::size_t value = 0;
                const auto *bytes = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
                for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                    value = value * 131 + bytes[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
        }
        freeifaddrs(ifa);
    }
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        return static_cast<net::node_t>(std::hash<std::string_view>{}(std::string_view{host}));
    }
    return 0;
}

} // namespace

/**
 * @brief Entry point verifying automatic node ID detection.
 */
int main() {
    const auto expect = compute_expected();
    net::init({0, 15000});
    const auto actual = net::local_node();
    assert(actual == expect);
    net::shutdown();
    return 0;
}
