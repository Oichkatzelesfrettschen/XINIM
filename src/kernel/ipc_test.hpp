/**
 * @file ipc_test.hpp
 * @brief Kernel-side IPC validation test interface
 *
 * @ingroup kernel
 */

#ifndef XINIM_KERNEL_IPC_TEST_HPP
#define XINIM_KERNEL_IPC_TEST_HPP

namespace xinim::kernel::test {

/**
 * @brief Run all IPC validation tests
 *
 * This function validates that IPC message structures are correct
 * and can be sent to servers. Should be called after servers are
 * spawned but before entering the scheduler loop.
 *
 * @return 0 on success, -1 on failure
 */
int run_ipc_validation_tests();

} // namespace xinim::kernel::test

#endif /* XINIM_KERNEL_IPC_TEST_HPP */
