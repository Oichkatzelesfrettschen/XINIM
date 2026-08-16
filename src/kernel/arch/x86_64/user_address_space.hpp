#pragma once

#include <cstddef>
#include <cstdint>

namespace xinim::kernel::x86_64 {

    inline constexpr uint64_t kPageSize = 4096U;
    inline constexpr uint64_t kUserCanonicalLimit = 0x0000800000000000ULL;

    enum class UserPageFlags : uint8_t {
        ReadOnly = 0U,
        Writable = 1U << 0U,
        Executable = 1U << 1U,
    };

    [[nodiscard]] constexpr UserPageFlags operator|(UserPageFlags left,
                                                    UserPageFlags right) noexcept {
        return static_cast<UserPageFlags>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
    }

    [[nodiscard]] constexpr bool has_flag(UserPageFlags value, UserPageFlags flag) noexcept {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0U;
    }

    struct UserAddressSpace {
        uint64_t root_physical{0U};
    };

    [[nodiscard]] bool create_user_address_space(UserAddressSpace &address_space) noexcept;

    [[nodiscard]] bool clone_user_address_space(const UserAddressSpace &source,
                                                UserAddressSpace &destination) noexcept;

    void destroy_user_address_space(UserAddressSpace &address_space) noexcept;

    [[nodiscard]] bool map_zeroed_user_page(UserAddressSpace &address_space,
                                            uint64_t virtual_address, UserPageFlags flags) noexcept;

    [[nodiscard]] bool unmap_user_page(UserAddressSpace &address_space,
                                       uint64_t virtual_address) noexcept;

    [[nodiscard]] bool copy_to_user_address_space(const UserAddressSpace &address_space,
                                                  uint64_t destination, const void *source,
                                                  size_t size) noexcept;

    [[nodiscard]] bool zero_user_address_space(const UserAddressSpace &address_space,
                                               uint64_t destination, size_t size) noexcept;

    [[nodiscard]] uint64_t resolve_user_physical(const UserAddressSpace &address_space,
                                                 uint64_t virtual_address) noexcept;

    [[nodiscard]] bool is_user_range_mapped(const UserAddressSpace &address_space,
                                            uint64_t virtual_address, size_t size,
                                            bool require_writable) noexcept;

    [[nodiscard]] bool is_user_range_executable(const UserAddressSpace &address_space,
                                                uint64_t virtual_address, size_t size) noexcept;

} // namespace xinim::kernel::x86_64
