// Ultra-minimal no-op kernel for testing repeated execution
// Does absolutely nothing except return
void kernel_main() {
    // No-op - just return immediately
    volatile int dummy = 42;  // Prevent optimization
    dummy = dummy + 1;
}
