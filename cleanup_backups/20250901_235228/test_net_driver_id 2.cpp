/**
 * @file test_net_driver_id.cpp
 * @brief Ensure automatic node identifier detection.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#if defined(_WIN32)
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <ifaddrs.h>
#include <net/if.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__)
#include <net/if_dl.h>
#else
#include <netpacket/packet.h>
#endif
#include <unistd.h>
#endif
#include <string_view>

namespace {

/**
 * @brief Replicate the driver's detection logic to obtain the expected
 *        identifier.
 */
[[nodiscard]] net::node_t compute_expected() {
#if defined(_WIN32)
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &size) == ERROR_BUFFER_OVERFLOW) {
        std::vector<unsigned char> buf(size);
        auto *aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, aa, &size) == NO_ERROR) {
            for (auto *cur = aa; cur; cur = cur->Next) {
                if (cur->OperStatus != IfOperStatusUp || cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    continue;
                }
                if (cur->PhysicalAddressLength > 0) {
                    std::size_t value = 0;
                    for (unsigned i = 0; i < cur->PhysicalAddressLength; ++i) {
                        value = value * 131 + cur->PhysicalAddress[i];
                    }
                    return static_cast<net::node_t>(value & 0x7fffffff);
                }
                for (auto *ua = cur->FirstUnicastAddress; ua; ua = ua->Next) {
                    auto fam = ua->Address.lpSockaddr->sa_family;
                    if (fam == AF_INET) {
                        auto *sin = reinterpret_cast<sockaddr_in *>(ua->Address.lpSockaddr);
                        std::size_t value = 0;
                        const auto *b = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
                        for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                            value = value * 131 + b[i];
                        }
                        return static_cast<net::node_t>(value & 0x7fffffff);
                    }
                    if (fam == AF_INET6) {
                        auto *sin6 = reinterpret_cast<sockaddr_in6 *>(ua->Address.lpSockaddr);
                        std::size_t value = 0;
                        const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
                        for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                            value = value * 131 + b[i];
                        }
                        return static_cast<net::node_t>(value & 0x7fffffff);
                    }
                }
            }
        }
    }
#else
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
            if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
                continue;
            }
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__)
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_LINK) {
                auto *sdl = reinterpret_cast<sockaddr_dl *>(cur->ifa_addr);
                if (sdl->sdl_alen > 0) {
                    std::size_t value = 0;
                    const auto *b = reinterpret_cast<const unsigned char *>(LLADDR(sdl));
                    for (int i = 0; i < sdl->sdl_alen; ++i) {
                        value = value * 131 + b[i];
                    }
                    freeifaddrs(ifa);
                    return static_cast<net::node_t>(value & 0x7fffffff);
                }
            }
#else
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
                auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
                std::size_t value = 0;
                for (int i = 0; i < ll->sll_halen; ++i) {
                    value = value * 131 + ll->sll_addr[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
#endif
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
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET6) {
                auto *sin6 = reinterpret_cast<sockaddr_in6 *>(cur->ifa_addr);
                std::size_t value = 0;
                const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
                for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                    value = value * 131 + b[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
        }
        freeifaddrs(ifa);
    }
#endif
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        auto value = std::hash<std::string_view>{}(std::string_view{host}) & 0x7fffffff;
        return static_cast<net::node_t>(value);
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
/**
 * @file test_net_driver_id.cpp
 * @brief Ensure automatic node identifier detection.
 */

#include "../kernel/net_driver.hpp"

#include <cassert>
#if defined(_WIN32)
#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#else
#include <ifaddrs.h>
#include <net/if.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__)
#include <net/if_dl.h>
#else
#include <netpacket/packet.h>
#endif
#include <unistd.h>
#endif
#include <string_view>

namespace {

/**
 * @brief Replicate the driver's detection logic to obtain the expected
 *        identifier.
 */
[[nodiscard]] net::node_t compute_expected() {
#if defined(_WIN32)
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &size) == ERROR_BUFFER_OVERFLOW) {
        std::vector<unsigned char> buf(size);
        auto *aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, aa, &size) == NO_ERROR) {
            for (auto *cur = aa; cur; cur = cur->Next) {
                if (cur->OperStatus != IfOperStatusUp || cur->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                    continue;
                }
                if (cur->PhysicalAddressLength > 0) {
                    std::size_t value = 0;
                    for (unsigned i = 0; i < cur->PhysicalAddressLength; ++i) {
                        value = value * 131 + cur->PhysicalAddress[i];
                    }
                    return static_cast<net::node_t>(value & 0x7fffffff);
                }
                for (auto *ua = cur->FirstUnicastAddress; ua; ua = ua->Next) {
                    auto fam = ua->Address.lpSockaddr->sa_family;
                    if (fam == AF_INET) {
                        auto *sin = reinterpret_cast<sockaddr_in *>(ua->Address.lpSockaddr);
                        std::size_t value = 0;
                        const auto *b = reinterpret_cast<const unsigned char *>(&sin->sin_addr);
                        for (unsigned i = 0; i < sizeof(sin->sin_addr); ++i) {
                            value = value * 131 + b[i];
                        }
                        return static_cast<net::node_t>(value & 0x7fffffff);
                    }
                    if (fam == AF_INET6) {
                        auto *sin6 = reinterpret_cast<sockaddr_in6 *>(ua->Address.lpSockaddr);
                        std::size_t value = 0;
                        const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
                        for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                            value = value * 131 + b[i];
                        }
                        return static_cast<net::node_t>(value & 0x7fffffff);
                    }
                }
            }
        }
    }
#else
    ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto *cur = ifa; cur != nullptr; cur = cur->ifa_next) {
            if (!(cur->ifa_flags & IFF_UP) || (cur->ifa_flags & IFF_LOOPBACK)) {
                continue;
            }
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||   \
    defined(__DragonFly__)
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_LINK) {
                auto *sdl = reinterpret_cast<sockaddr_dl *>(cur->ifa_addr);
                if (sdl->sdl_alen > 0) {
                    std::size_t value = 0;
                    const auto *b = reinterpret_cast<const unsigned char *>(LLADDR(sdl));
                    for (int i = 0; i < sdl->sdl_alen; ++i) {
                        value = value * 131 + b[i];
                    }
                    freeifaddrs(ifa);
                    return static_cast<net::node_t>(value & 0x7fffffff);
                }
            }
#else
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_PACKET) {
                auto *ll = reinterpret_cast<sockaddr_ll *>(cur->ifa_addr);
                std::size_t value = 0;
                for (int i = 0; i < ll->sll_halen; ++i) {
                    value = value * 131 + ll->sll_addr[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
#endif
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
            if (cur->ifa_addr && cur->ifa_addr->sa_family == AF_INET6) {
                auto *sin6 = reinterpret_cast<sockaddr_in6 *>(cur->ifa_addr);
                std::size_t value = 0;
                const auto *b = reinterpret_cast<const unsigned char *>(&sin6->sin6_addr);
                for (unsigned i = 0; i < sizeof(sin6->sin6_addr); ++i) {
                    value = value * 131 + b[i];
                }
                freeifaddrs(ifa);
                return static_cast<net::node_t>(value & 0x7fffffff);
            }
        }
        freeifaddrs(ifa);
    }
#endif
    char host[256]{};
    if (gethostname(host, sizeof(host)) == 0) {
        auto value = std::hash<std::string_view>{}(std::string_view{host}) & 0x7fffffff;
        return static_cast<net::node_t>(value);
    }
    return 0;
}

} // namespace

/**
 * @brief Entry point verifying automatic node ID detection.
 */
int main() {
    const auto expect = compute_expected();
    net::driver.init({0, 15000});
    const auto actual = net::driver.local_node();
    assert(actual == expect);
    net::driver.shutdown();
    return 0;
}
