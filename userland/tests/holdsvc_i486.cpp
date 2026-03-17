#include <stdint.h>

extern "C" int xinim_user_main(int argc, char** argv, char** envp) noexcept {
    (void)argc;
    (void)argv;
    (void)envp;

    volatile uint32_t counter = 0U;
    for (;;) {
        counter += 1U;
        asm volatile("" : "+m"(counter));
    }
}
