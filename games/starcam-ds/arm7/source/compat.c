// Provided by newer picolibc releases; defined here so the game also links
// against older toolchains. The DS has a single core per CPU, so a no-op is fine.
__attribute__((weak)) void __sync_synchronize_none(void) {}
