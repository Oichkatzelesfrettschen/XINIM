#pragma once

#include <cstdint>

namespace xinim::kernel {
    struct FileDescriptor;
}

namespace xinim::kernel::x86_64 {

    [[nodiscard]] bool socket_descriptor_is_socket(
        const xinim::kernel::FileDescriptor &descriptor) noexcept;
    void socket_descriptor_retain(const xinim::kernel::FileDescriptor &descriptor) noexcept;
    void socket_descriptor_release(xinim::kernel::FileDescriptor &descriptor) noexcept;

    [[nodiscard]] int64_t socket_create(int domain, int type, int protocol) noexcept;
    [[nodiscard]] int64_t socket_bind(int descriptor, uintptr_t address,
                                      uint32_t address_length) noexcept;
    [[nodiscard]] int64_t socket_connect(int descriptor, uintptr_t address,
                                         uint32_t address_length) noexcept;
    [[nodiscard]] int64_t socket_listen(int descriptor, int backlog) noexcept;
    [[nodiscard]] int64_t socket_accept(int descriptor, uintptr_t address,
                                        uintptr_t address_length) noexcept;
    [[nodiscard]] int64_t socket_sendto(int descriptor, uintptr_t buffer, uint32_t length,
                                        int flags, uintptr_t address,
                                        uint32_t address_length) noexcept;
    [[nodiscard]] int64_t socket_recvfrom(int descriptor, uintptr_t buffer, uint32_t length,
                                          int flags, uintptr_t address,
                                          uintptr_t address_length) noexcept;
    [[nodiscard]] int64_t socket_shutdown(int descriptor, int how) noexcept;
    [[nodiscard]] int64_t socket_setsockopt(int descriptor, int level, int option,
                                            uintptr_t value, uint32_t value_length) noexcept;
    [[nodiscard]] int64_t socket_getsockopt(int descriptor, int level, int option,
                                            uintptr_t value, uintptr_t value_length) noexcept;
    [[nodiscard]] int64_t socket_getsockname(int descriptor, uintptr_t address,
                                             uintptr_t address_length) noexcept;
    [[nodiscard]] int64_t socket_getpeername(int descriptor, uintptr_t address,
                                             uintptr_t address_length) noexcept;
    [[nodiscard]] int64_t socket_sendmsg(int descriptor, uintptr_t message, int flags) noexcept;
    [[nodiscard]] int64_t socket_recvmsg(int descriptor, uintptr_t message, int flags) noexcept;
    [[nodiscard]] int64_t socket_socketpair(int domain, int type, int protocol,
                                            uintptr_t descriptors) noexcept;

    [[nodiscard]] int64_t socket_read(int descriptor, uintptr_t buffer, uint32_t length) noexcept;
    [[nodiscard]] int64_t socket_write(int descriptor, uintptr_t buffer,
                                       uint32_t length) noexcept;
    [[nodiscard]] bool socket_descriptor_read_ready(int descriptor) noexcept;
    [[nodiscard]] bool socket_descriptor_write_ready(int descriptor) noexcept;

} // namespace xinim::kernel::x86_64
