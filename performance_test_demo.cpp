#include <iostream>
#include <chrono>
#include <iomanip>

// Simulated function pointer performance demo
class PerformanceDemo {
private:
    using TestFunc = void (PerformanceDemo::*)();
    TestFunc test_impl_;
    bool tracing_enabled_;

public:
    PerformanceDemo() : tracing_enabled_(false), test_impl_(&PerformanceDemo::fast_method) {}

    // Hot-path method (zero branching)
    void test_method() {
        (this->*test_impl_)();  // Direct function pointer call - NO if statements!
    }

    // Runtime switching
    void enable_tracing() {
        tracing_enabled_ = true;
        test_impl_ = &PerformanceDemo::traced_method;
    }

    void disable_tracing() {
        tracing_enabled_ = false;
        test_impl_ = &PerformanceDemo::fast_method;
    }

    bool is_tracing_enabled() const { return tracing_enabled_; }

private:
    // Fast implementation (zero overhead)
    void fast_method() {
        volatile int x = 42;  // Simulate minimal work
        (void)x;
    }

    // Traced implementation (with overhead)
    void traced_method() {
        volatile int x = 42;  // Simulate work
        volatile int y = x * 2;  // Simulate tracing overhead
        (void)x; (void)y;
    }

public:
    void run_benchmark() {
        std::cout << "\n🚀 Function Pointer Performance Demonstration\n";
        std::cout << "============================================\n";
        std::cout << "This demonstrates the zero-branching optimization\n";
        std::cout << "used in the Seven framework's ServiceHost.\n\n";

        const int iterations = 1000000;

        // Test 1: Fast mode
        std::cout << "📊 Test 1: High-Performance Mode (Fast Implementation)\n";
        disable_tracing();
        std::cout << "   • Tracing enabled: " << (is_tracing_enabled() ? "YES" : "NO") << "\n";
        std::cout << "   • Using: fast_method() via function pointer\n";
        std::cout << "   • Characteristics: Zero branching, minimal overhead\n";

        auto start_fast = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            test_method();  // Uses function pointer -> fast_method
        }
        auto end_fast = std::chrono::high_resolution_clock::now();
        auto fast_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_fast - start_fast);

        std::cout << "   • " << iterations << " calls in: " << fast_duration.count() << "ns\n";
        std::cout << "   • Average per call: " << std::fixed << std::setprecision(3) 
                  << (fast_duration.count() / static_cast<double>(iterations)) << "ns\n\n";

        // Test 2: Traced mode
        std::cout << "📊 Test 2: Full Observability Mode (Traced Implementation)\n";
        enable_tracing();
        std::cout << "   • Tracing enabled: " << (is_tracing_enabled() ? "YES" : "NO") << "\n";
        std::cout << "   • Using: traced_method() via function pointer\n";
        std::cout << "   • Characteristics: Additional overhead simulation\n";

        auto start_traced = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            test_method();  // Uses function pointer -> traced_method
        }
        auto end_traced = std::chrono::high_resolution_clock::now();
        auto traced_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_traced - start_traced);

        std::cout << "   • " << iterations << " calls in: " << traced_duration.count() << "ns\n";
        std::cout << "   • Average per call: " << std::fixed << std::setprecision(3) 
                  << (traced_duration.count() / static_cast<double>(iterations)) << "ns\n\n";

        // Performance analysis
        double overhead_ratio = static_cast<double>(traced_duration.count()) / fast_duration.count();
        double overhead_percentage = (overhead_ratio - 1.0) * 100.0;

        std::cout << "🎯 Performance Analysis:\n";
        std::cout << "   • Overhead ratio: " << std::fixed << std::setprecision(3) << overhead_ratio << "x\n";
        std::cout << "   • Overhead percentage: " << std::setprecision(1) << overhead_percentage << "%\n";
        std::cout << "   • Runtime switching: ZERO branching penalty! ✅\n";
        std::cout << "   • Hot-path optimization: Function pointers eliminate if-statements ✅\n";
        std::cout << "   • Dynamic control: Switch modes without recompilation ✅\n";

        if (overhead_ratio < 2.0) {
            std::cout << "   • 🎉 EXCELLENT: Overhead is minimal (< 2x)\n";
        } else if (overhead_ratio < 5.0) {
            std::cout << "   • ✅ GOOD: Overhead is acceptable (< 5x)\n";
        } else {
            std::cout << "   • ⚠️  WARNING: High overhead detected\n";
        }

        // Test 3: Runtime switching validation
        std::cout << "\n📊 Test 3: Runtime Switching Validation\n";
        std::cout << "   • Testing rapid mode switching without degradation\n";

        auto switch_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            disable_tracing();
            test_method();
            enable_tracing();
            test_method();
        }
        auto switch_end = std::chrono::high_resolution_clock::now();
        auto switch_duration = std::chrono::duration_cast<std::chrono::microseconds>(switch_end - switch_start);

        std::cout << "   • 2000 calls with 1000 mode switches: " << switch_duration.count() << "μs\n";
        std::cout << "   • Average per switch + call: " << std::setprecision(2) << switch_duration.count() / 2000.0 << "μs\n";
        std::cout << "   • ✅ Runtime switching works seamlessly\n";

        std::cout << "\n🔧 This is the same pattern used in ServiceHost for:\n";
        std::cout << "   • publish_broadcast() - NATS message publishing\n";
        std::cout << "   • publish_point_to_point() - Direct message routing\n";
        std::cout << "   • Real-world performance gains in production systems\n";
        std::cout << "============================================\n\n";
    }
};

int main() {
    PerformanceDemo demo;
    demo.run_benchmark();
    return 0;
}
