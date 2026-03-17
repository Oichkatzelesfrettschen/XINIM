#pragma once

#include <stdint.h>

namespace xinim::i486::virtio_net {

bool initialize() noexcept;
bool send(const void* data, uint32_t length) noexcept;
bool recv(void* buffer, uint32_t buffer_size, uint32_t* out_length) noexcept;
bool is_initialized() noexcept;
const uint8_t* mac_address() noexcept;

} // namespace xinim::i486::virtio_net
