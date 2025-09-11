/**
 * @file kernel.hpp
 * @brief XINIM Kernel Interface
 */

#ifndef XINIM_KERNEL_HPP
#define XINIM_KERNEL_HPP

#include <cstdint>
#include <memory>

namespace xinim {
namespace kernel {

class Kernel {
public:
    Kernel();
    ~Kernel();

    bool initialize();
    void run();
    void shutdown();

private:
    bool initialized_;
};

} // namespace kernel
} // namespace xinim

#endif // XINIM_KERNEL_HPP
