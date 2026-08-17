#include "socket_syscalls.hpp"

#include "../../fd_table.hpp"
#include "../../pcb.hpp"
#include "../../scheduler.hpp"
#include "../../uaccess.hpp"
#include "process_syscalls.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xinim::kernel::x86_64 {
    namespace {

        constexpr int kAddressFamilyInet = 2;
        constexpr int kSocketTypeDatagram = 2;
        constexpr int kSocketTypeMask = 0xf;
        constexpr int kSocketCloexec = 0x80000;
        constexpr int kSocketNonblock = 0x800;
        constexpr int kSocketOptionLevel = 1;
        constexpr int kSocketOptionReuseAddress = 2;
        constexpr int kShutdownRead = 0;
        constexpr int kShutdownWrite = 1;
        constexpr int kShutdownBoth = 2;
        constexpr uint32_t kAnyAddress = 0U;
        constexpr uint32_t kLoopbackAddress = 0x0100007fU;
        constexpr uint16_t kFirstEphemeralPort = 49152U;
        constexpr std::size_t kSocketObjectCapacity = 32U;
        constexpr std::size_t kDatagramQueueCapacity = 8U;
        constexpr std::size_t kMaximumDatagramSize = 512U;

        struct [[gnu::packed]] SocketAddressIn {
            uint16_t family;
            uint16_t port;
            uint32_t address;
            std::array<uint8_t, 8U> zero;
        };

        static_assert(sizeof(SocketAddressIn) == 16U);

        struct Datagram {
            uint16_t length;
            uint16_t source_port;
            uint32_t source_address;
            std::array<uint8_t, kMaximumDatagramSize> data;
        };

        struct SocketObject {
            bool in_use;
            bool bound;
            bool connected;
            bool shutdown_read;
            bool shutdown_write;
            bool reuse_address;
            uint32_t references;
            uint32_t local_address;
            uint32_t remote_address;
            uint16_t local_port;
            uint16_t remote_port;
            uint32_t status_flags;
            uint32_t receive_head;
            uint32_t receive_tail;
            uint32_t receive_count;
            std::array<Datagram, kDatagramQueueCapacity> receive_queue;
        };

        uint8_t g_socket_inode;
        std::array<SocketObject, kSocketObjectCapacity> g_socket_objects;
        uint16_t g_next_ephemeral_port = kFirstEphemeralPort;

        [[nodiscard]] uint16_t network_to_host16(uint16_t value) noexcept {
            return static_cast<uint16_t>((value >> 8U) | (value << 8U));
        }

        [[nodiscard]] uint16_t host_to_network16(uint16_t value) noexcept {
            return network_to_host16(value);
        }

        [[nodiscard]] bool address_is_supported(uint32_t address) noexcept {
            return address == kAnyAddress || address == kLoopbackAddress;
        }

        [[nodiscard]] bool address_matches(uint32_t bound_address,
                                           uint32_t destination_address) noexcept {
            return bound_address == kAnyAddress || bound_address == destination_address;
        }

        [[nodiscard]] SocketObject *object_from_descriptor(
            const xinim::kernel::FileDescriptor &descriptor) noexcept {
            if (descriptor.inode != &g_socket_inode || descriptor.private_data == nullptr) {
                return nullptr;
            }
            auto *object = static_cast<SocketObject *>(descriptor.private_data);
            return object->in_use ? object : nullptr;
        }

        [[nodiscard]] xinim::kernel::FileDescriptor *descriptor_for_fd(int descriptor) noexcept {
            xinim::kernel::ProcessControlBlock *process = xinim::kernel::get_current_process();
            if (process == nullptr || !process->fd_table.is_valid_fd(descriptor)) {
                return nullptr;
            }
            return process->fd_table.get_fd(descriptor);
        }

        [[nodiscard]] SocketObject *object_for_fd(int descriptor) noexcept {
            xinim::kernel::FileDescriptor *file_descriptor = descriptor_for_fd(descriptor);
            return file_descriptor == nullptr ? nullptr : object_from_descriptor(*file_descriptor);
        }

        [[nodiscard]] int copy_address_from_user(uintptr_t address_pointer,
                                                 uint32_t address_length,
                                                 SocketAddressIn &address) noexcept {
            if (address_length < sizeof(SocketAddressIn)) {
                return -EINVAL;
            }
            if (xinim::kernel::copy_from_user(&address, address_pointer, sizeof(address)) != 0) {
                return -EFAULT;
            }
            if (address.family != kAddressFamilyInet) {
                return -EAFNOSUPPORT;
            }
            if (!address_is_supported(address.address)) {
                return -EADDRNOTAVAIL;
            }
            return 0;
        }

        [[nodiscard]] int read_output_length(uintptr_t address_length,
                                             uint32_t &capacity) noexcept {
            if (address_length == 0U ||
                xinim::kernel::copy_from_user(&capacity, address_length, sizeof(capacity)) != 0) {
                return -EFAULT;
            }
            if (capacity < sizeof(SocketAddressIn)) {
                return -EINVAL;
            }
            return 0;
        }

        [[nodiscard]] int write_address_to_user(uintptr_t address_pointer,
                                                uintptr_t address_length,
                                                const SocketAddressIn &address) noexcept {
            if (address_pointer == 0U && address_length == 0U) {
                return 0;
            }
            uint32_t capacity = 0U;
            const int length_result = read_output_length(address_length, capacity);
            if (length_result != 0) {
                return length_result;
            }
            if (xinim::kernel::copy_to_user(address_pointer, &address, sizeof(address)) != 0 ||
                xinim::kernel::copy_to_user(address_length, &capacity, sizeof(capacity)) != 0) {
                return -EFAULT;
            }
            return 0;
        }

        [[nodiscard]] bool port_in_use(uint16_t port, uint32_t address,
                                       const SocketObject *ignored) noexcept {
            for (SocketObject &object : g_socket_objects) {
                if (!object.in_use || !object.bound || &object == ignored ||
                    object.local_port != port) {
                    continue;
                }
                if (address_matches(object.local_address, address) ||
                    address_matches(address, object.local_address)) {
                    if (!object.reuse_address ||
                        (ignored != nullptr && !ignored->reuse_address)) {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] uint16_t allocate_ephemeral_port(const SocketObject *ignored) noexcept {
            for (std::size_t attempt = 0U; attempt < 16384U; ++attempt) {
                const uint16_t candidate = g_next_ephemeral_port;
                ++g_next_ephemeral_port;
                if (g_next_ephemeral_port < kFirstEphemeralPort) {
                    g_next_ephemeral_port = kFirstEphemeralPort;
                }
                if (!port_in_use(candidate, kLoopbackAddress, ignored)) {
                    return candidate;
                }
            }
            return 0U;
        }

        [[nodiscard]] int ensure_bound(SocketObject &object) noexcept {
            if (object.bound) {
                return 0;
            }
            const uint16_t port = allocate_ephemeral_port(&object);
            if (port == 0U) {
                return -EADDRINUSE;
            }
            object.local_address = kLoopbackAddress;
            object.local_port = port;
            object.bound = true;
            return 0;
        }

        [[nodiscard]] SocketObject *allocate_socket_object() noexcept {
            for (SocketObject &object : g_socket_objects) {
                if (!object.in_use) {
                    object = {};
                    object.in_use = true;
                    object.references = 1U;
                    return &object;
                }
            }
            return nullptr;
        }

        [[nodiscard]] SocketObject *recipient_for(uint32_t address, uint16_t port) noexcept {
            for (SocketObject &object : g_socket_objects) {
                if (object.in_use && object.bound && object.local_port == port &&
                    address_matches(object.local_address, address)) {
                    return &object;
                }
            }
            return nullptr;
        }

        [[nodiscard]] int enqueue_datagram(SocketObject &recipient, const uint8_t *data,
                                           uint32_t length, uint32_t source_address,
                                           uint16_t source_port) noexcept {
            if (recipient.receive_count >= kDatagramQueueCapacity) {
                return -ENOBUFS;
            }
            Datagram &datagram = recipient.receive_queue[recipient.receive_tail];
            datagram.length = static_cast<uint16_t>(length);
            datagram.source_address = source_address;
            datagram.source_port = source_port;
            if (length != 0U) {
                std::memcpy(datagram.data.data(), data, length);
            }
            recipient.receive_tail =
                (recipient.receive_tail + 1U) % kDatagramQueueCapacity;
            ++recipient.receive_count;
            xinim::kernel::x86_64::wake_io_waiters();
            return 0;
        }

        [[nodiscard]] SocketAddressIn make_address(uint32_t address, uint16_t port) noexcept {
            SocketAddressIn result{};
            result.family = kAddressFamilyInet;
            result.port = host_to_network16(port);
            result.address = address;
            return result;
        }

        [[nodiscard]] int64_t send_datagram(SocketObject &sender, uintptr_t buffer,
                                            uint32_t length, uintptr_t address,
                                            uint32_t address_length) noexcept {
            if (sender.shutdown_write) {
                return -ESHUTDOWN;
            }
            if (length > kMaximumDatagramSize) {
                return -EMSGSIZE;
            }
            const int bound_result = ensure_bound(sender);
            if (bound_result != 0) {
                return bound_result;
            }

            uint32_t destination_address = sender.remote_address;
            uint16_t destination_port = sender.remote_port;
            if (address != 0U) {
                SocketAddressIn destination{};
                const int address_result =
                    copy_address_from_user(address, address_length, destination);
                if (address_result != 0) {
                    return address_result;
                }
                destination_address = destination.address;
                destination_port = network_to_host16(destination.port);
            } else if (!sender.connected) {
                return -EDESTADDRREQ;
            }
            if (destination_port == 0U) {
                return -ECONNREFUSED;
            }

            std::array<uint8_t, kMaximumDatagramSize> transfer{};
            if (length != 0U && xinim::kernel::copy_from_user(transfer.data(), buffer, length) != 0) {
                return -EFAULT;
            }
            SocketObject *recipient = recipient_for(destination_address, destination_port);
            if (recipient == nullptr) {
                return -ECONNREFUSED;
            }
            const int enqueue_result = enqueue_datagram(
                *recipient, transfer.data(), length, sender.local_address, sender.local_port);
            return enqueue_result == 0 ? static_cast<int64_t>(length)
                                       : static_cast<int64_t>(enqueue_result);
        }

    } // namespace

    bool socket_descriptor_is_socket(
        const xinim::kernel::FileDescriptor &descriptor) noexcept {
        return object_from_descriptor(descriptor) != nullptr;
    }

    void socket_descriptor_retain(const xinim::kernel::FileDescriptor &descriptor) noexcept {
        SocketObject *object = object_from_descriptor(descriptor);
        if (object != nullptr && object->references != UINT32_MAX) {
            ++object->references;
        }
    }

    void socket_descriptor_release(xinim::kernel::FileDescriptor &descriptor) noexcept {
        SocketObject *object = object_from_descriptor(descriptor);
        if (object == nullptr || object->references == 0U) {
            return;
        }
        --object->references;
        if (object->references == 0U) {
            *object = {};
        }
    }

    int64_t socket_create(int domain, int type, int protocol) noexcept {
        if (domain != kAddressFamilyInet) {
            return -EAFNOSUPPORT;
        }
        if ((type & kSocketTypeMask) != kSocketTypeDatagram || protocol != 0) {
            return -EPROTONOSUPPORT;
        }
        xinim::kernel::ProcessControlBlock *process = xinim::kernel::get_current_process();
        if (process == nullptr) {
            return -ESRCH;
        }
        const int descriptor_number = process->fd_table.allocate_fd();
        if (descriptor_number < 0) {
            return descriptor_number;
        }
        if (static_cast<uint64_t>(descriptor_number) >= process->descriptor_limit) {
            static_cast<void>(process->fd_table.close_fd(descriptor_number));
            return -EMFILE;
        }
        SocketObject *object = allocate_socket_object();
        if (object == nullptr) {
            static_cast<void>(process->fd_table.close_fd(descriptor_number));
            return -ENFILE;
        }
        xinim::kernel::FileDescriptor *descriptor =
            process->fd_table.get_fd(descriptor_number);
        descriptor->inode = &g_socket_inode;
        descriptor->private_data = object;
        descriptor->file_flags = (type & kSocketNonblock) != 0 ? kSocketNonblock : 0U;
        descriptor->flags = (type & kSocketCloexec) != 0
                                ? static_cast<uint32_t>(xinim::kernel::FdFlags::CLOEXEC)
                                : 0U;
        return descriptor_number;
    }

    int64_t socket_bind(int descriptor, uintptr_t address, uint32_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (object->bound) {
            return -EINVAL;
        }
        SocketAddressIn requested{};
        const int address_result = copy_address_from_user(address, address_length, requested);
        if (address_result != 0) {
            return address_result;
        }
        uint16_t port = network_to_host16(requested.port);
        if (port == 0U) {
            port = allocate_ephemeral_port(object);
            if (port == 0U) {
                return -EADDRINUSE;
            }
        } else if (port_in_use(port, requested.address, object)) {
            return -EADDRINUSE;
        }
        object->local_address = requested.address;
        object->local_port = port;
        object->bound = true;
        return 0;
    }

    int64_t socket_connect(int descriptor, uintptr_t address,
                           uint32_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        SocketAddressIn requested{};
        const int address_result = copy_address_from_user(address, address_length, requested);
        if (address_result != 0) {
            return address_result;
        }
        const uint16_t port = network_to_host16(requested.port);
        if (port == 0U) {
            return -EINVAL;
        }
        const int bound_result = ensure_bound(*object);
        if (bound_result != 0) {
            return bound_result;
        }
        object->remote_address = requested.address;
        object->remote_port = port;
        object->connected = true;
        return 0;
    }

    int64_t socket_listen(int descriptor, int /*backlog*/) noexcept {
        return object_for_fd(descriptor) == nullptr ? -EBADF : -EOPNOTSUPP;
    }

    int64_t socket_accept(int descriptor, uintptr_t /*address*/,
                          uintptr_t /*address_length*/) noexcept {
        return object_for_fd(descriptor) == nullptr ? -EBADF : -EOPNOTSUPP;
    }

    int64_t socket_sendto(int descriptor, uintptr_t buffer, uint32_t length, int flags,
                          uintptr_t address, uint32_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (flags != 0) {
            return -EINVAL;
        }
        return send_datagram(*object, buffer, length, address, address_length);
    }

    int64_t socket_recvfrom(int descriptor, uintptr_t buffer, uint32_t length, int flags,
                            uintptr_t address, uintptr_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (flags != 0) {
            return -EINVAL;
        }
        if (object->shutdown_read) {
            return 0;
        }
        if (object->receive_count == 0U) {
            return -EAGAIN;
        }

        Datagram &datagram = object->receive_queue[object->receive_head];
        if (length < datagram.length && buffer == 0U) {
            return -EFAULT;
        }
        const uint32_t transferred = length < datagram.length ? length : datagram.length;
        if (transferred != 0U &&
            xinim::kernel::copy_to_user(buffer, datagram.data.data(), transferred) != 0) {
            return -EFAULT;
        }
        if (address != 0U || address_length != 0U) {
            const SocketAddressIn source = make_address(datagram.source_address,
                                                        datagram.source_port);
            const int address_result = write_address_to_user(address, address_length, source);
            if (address_result != 0) {
                return address_result;
            }
        }
        object->receive_head = (object->receive_head + 1U) % kDatagramQueueCapacity;
        --object->receive_count;
        xinim::kernel::x86_64::wake_io_waiters();
        return static_cast<int64_t>(transferred);
    }

    int64_t socket_shutdown(int descriptor, int how) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (how < kShutdownRead || how > kShutdownBoth) {
            return -EINVAL;
        }
        if (how == kShutdownRead || how == kShutdownBoth) {
            object->shutdown_read = true;
        }
        if (how == kShutdownWrite || how == kShutdownBoth) {
            object->shutdown_write = true;
        }
        return 0;
    }

    int64_t socket_setsockopt(int descriptor, int level, int option, uintptr_t value,
                              uint32_t value_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (level != kSocketOptionLevel || option != kSocketOptionReuseAddress) {
            return -ENOPROTOOPT;
        }
        if (value == 0U || value_length < sizeof(int)) {
            return -EFAULT;
        }
        int requested = 0;
        if (xinim::kernel::copy_from_user(&requested, value, sizeof(requested)) != 0) {
            return -EFAULT;
        }
        object->reuse_address = requested != 0;
        return 0;
    }

    int64_t socket_getsockopt(int descriptor, int level, int option, uintptr_t value,
                              uintptr_t value_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (level != kSocketOptionLevel || option != kSocketOptionReuseAddress) {
            return -ENOPROTOOPT;
        }
        uint32_t capacity = 0U;
        if (value == 0U || value_length == 0U ||
            xinim::kernel::copy_from_user(&capacity, value_length, sizeof(capacity)) != 0) {
            return -EFAULT;
        }
        if (capacity < sizeof(int)) {
            return -EINVAL;
        }
        const int result = object->reuse_address ? 1 : 0;
        capacity = sizeof(int);
        if (xinim::kernel::copy_to_user(value, &result, sizeof(result)) != 0 ||
            xinim::kernel::copy_to_user(value_length, &capacity, sizeof(capacity)) != 0) {
            return -EFAULT;
        }
        return 0;
    }

    int64_t socket_getsockname(int descriptor, uintptr_t address,
                               uintptr_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        const SocketAddressIn local = make_address(object->local_address, object->local_port);
        return write_address_to_user(address, address_length, local);
    }

    int64_t socket_getpeername(int descriptor, uintptr_t address,
                               uintptr_t address_length) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        if (object == nullptr) {
            return -EBADF;
        }
        if (!object->connected) {
            return -ENOTCONN;
        }
        const SocketAddressIn remote = make_address(object->remote_address, object->remote_port);
        return write_address_to_user(address, address_length, remote);
    }

    int64_t socket_sendmsg(int descriptor, uintptr_t /*message*/, int /*flags*/) noexcept {
        return object_for_fd(descriptor) == nullptr ? -EBADF : -EOPNOTSUPP;
    }

    int64_t socket_recvmsg(int descriptor, uintptr_t /*message*/, int /*flags*/) noexcept {
        return object_for_fd(descriptor) == nullptr ? -EBADF : -EOPNOTSUPP;
    }

    int64_t socket_socketpair(int /*domain*/, int /*type*/, int /*protocol*/,
                             uintptr_t /*descriptors*/) noexcept {
        return -EAFNOSUPPORT;
    }

    int64_t socket_read(int descriptor, uintptr_t buffer, uint32_t length) noexcept {
        return socket_recvfrom(descriptor, buffer, length, 0, 0U, 0U);
    }

    int64_t socket_write(int descriptor, uintptr_t buffer, uint32_t length) noexcept {
        return socket_sendto(descriptor, buffer, length, 0, 0U, 0U);
    }

    bool socket_descriptor_read_ready(int descriptor) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        return object != nullptr && (object->shutdown_read || object->receive_count != 0U);
    }

    bool socket_descriptor_write_ready(int descriptor) noexcept {
        SocketObject *object = object_for_fd(descriptor);
        return object != nullptr && !object->shutdown_write &&
               object->receive_count < kDatagramQueueCapacity;
    }

} // namespace xinim::kernel::x86_64
