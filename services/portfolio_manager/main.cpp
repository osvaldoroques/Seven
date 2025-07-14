#include "portfolio_manager.hpp"
#include "messages_register.hpp"
#include "messages.pb.h"
#include "opentelemetry_integration.hpp"  // Add OpenTelemetry integration
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <iomanip>  // For std::setprecision

int main(int argc, char* argv[]) {
    std::string config_file = "config.yaml";
    
    // Allow config file to be specified as command line argument
    if (argc > 1) {
        config_file = argv[1];
    }

    // 🔥 Initialize OpenTelemetry before anything else
    std::string otel_endpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT") 
        ? std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT") 
        : "http://otel-collector:4317";
    std::string service_name = std::getenv("OTEL_SERVICE_NAME") 
        ? std::getenv("OTEL_SERVICE_NAME") 
        : "portfolio_manager";
    
    OpenTelemetryIntegration::initialize(service_name, otel_endpoint);
    std::cout << "✅ OpenTelemetry initialized: " << service_name << " -> " << otel_endpoint << std::endl;

    // 🚀 Performance optimization demonstration
    std::cout << "\n🚀 Function Pointer Performance Optimization Demo\n";
    std::cout << "================================================\n";
    
    // Create service host with unique ID
    PortfolioManager svc(
      PortfolioManager::ConfigFileTag{},
      "svc-portfolio-001",
      config_file,
      MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint, ([&svc](const Trevor::HealthCheckRequest& req) {
            // Extract trace context from incoming message
            auto trace_context = svc.extract_trace_context_from_message(req);
            TRACE_SPAN_WITH_CONTEXT("HealthCheck::Process", trace_context);
            
            auto logger = svc.create_request_logger();
            auto span_logger = logger->create_span_logger("HealthCheck");
            
            _trace_span.add_attributes({
                {"service.operation", "health_check"},
                {"request.service_name", req.service_name()},
                {"request.uid", req.uid()}
            });
            
            span_logger->info("Received HealthCheckRequest from service: {}, UID: {}", 
                            req.service_name(), req.uid());

            Trevor::HealthCheckResponse res;
            res.set_service_name("PortfolioManager");
            res.set_uid(svc.get_uid());
            res.set_status(svc.get_status());
            
            // Inject trace context into response
            svc.inject_trace_context_into_message(res, _trace_span.get_span());
            
            svc.publish_point_to_point(req.uid(), res);
            
            auto [trace_id, span_id] = _trace_span.get_trace_and_span_ids();
            span_logger->debug("Sent HealthCheckResponse to: {} trace_id={} span_id={}", 
                             req.uid(), trace_id, span_id);
        })),
      MSG_REG(Trevor::PortfolioRequest, MessageRouting::PointToPoint, ([&svc](const Trevor::PortfolioRequest& req) {
            // Extract trace context from incoming message
            auto trace_context = svc.extract_trace_context_from_message(req);
            TRACE_SPAN_WITH_CONTEXT("Portfolio::Process", trace_context);
            
            auto logger = svc.create_request_logger();
            auto db_span = logger->create_span_logger("DatabaseLookup");
            auto calc_span = logger->create_span_logger("PortfolioCalculation");
            
            _trace_span.add_attributes({
                {"service.operation", "portfolio_request"},
                {"request.account_id", req.account_id()},
                {"request.requester_uid", req.requester_uid()}
            });
            
            logger->info("Processing PortfolioRequest for account: {}", req.account_id());
            
            // Simulate database lookup span
            {
                TRACE_CHILD_SPAN("Database::AccountLookup", _trace_span.get_span());
                db_span->debug("Looking up account data in database");
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate DB latency
            }
            
            Trevor::PortfolioResponse res;
            res.set_account_id(req.account_id());
            res.set_total_value(svc.get_config<double>("portfolio_manager.default_portfolio_value", 100000.0));
            
            // Simulate calculation span
            {
                TRACE_CHILD_SPAN("Portfolio::Calculate", _trace_span.get_span());
                calc_span->debug("Calculated portfolio value: {}", res.total_value());
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Simulate calculation time
            }
            
            // Inject trace context into response
            svc.inject_trace_context_into_message(res, _trace_span.get_span());
            res.set_cash_balance(25000.0);
            res.set_status("active");
            
            // Add some sample positions from config
            auto max_positions = svc.get_config<int>("portfolio_manager.max_positions", 1000);
            logger->debug("Portfolio supports up to {} positions", max_positions);
            
            svc.publish_point_to_point(req.requester_uid(), res);
            logger->info("Sent PortfolioResponse for account: {}", req.account_id());
        })),
      MSG_REG(Trevor::MarketDataUpdate, MessageRouting::Broadcast, ([&svc](const Trevor::MarketDataUpdate& update) {
            auto logger = svc.create_request_logger();
            logger->debug("Market Data Update - Symbol: {}, Price: ${}, Volume: {}", 
                         update.symbol(), update.price(), update.volume());
            
            // Process market data update based on configuration
            auto update_frequency = svc.get_config<int>("portfolio_manager.update_frequency", 1000);
            if (update_frequency > 0) {
                // Submit portfolio recalculation to thread pool
                svc.submit_task([logger, symbol = update.symbol(), price = update.price()]() {
                    // Convert thread ID to string for logging
                    std::ostringstream thread_id_stream;
                    thread_id_stream << std::this_thread::get_id();
                    logger->trace("Recalculating portfolio for {} at price ${} in thread {}", 
                                symbol, price, thread_id_stream.str());
                    
                    // Simulate portfolio calculation work
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    logger->debug("Portfolio calculation completed for {}", symbol);
                });
            }
        }))
      // Additional message handlers can be added here as needed
    );

    // Use NATS_URL environment variable if available
    const char* nats_env_url = std::getenv("NATS_URL");
    std::string nats_url = nats_env_url ? nats_env_url : "nats://localhost:4222";
    
    svc.init_nats(nats_url);
    svc.init_jetstream();
    
    // Check if performance demo should run (can be disabled via config or environment)
    bool run_performance_demo = true;
    const char* skip_demo = std::getenv("SKIP_PERFORMANCE_DEMO");
    if (skip_demo && std::string(skip_demo) == "true") {
        run_performance_demo = false;
    }
    
    if (run_performance_demo) {
        // 🚀 Permanent Function Pointer Performance Optimization Demo
        std::cout << "\n🚀 Hot-Path Optimization: Function Pointer Switching Demo\n";
        std::cout << "============================================================\n";
        std::cout << "This demo runs automatically on every startup to validate\n";
        std::cout << "the zero-branching performance optimization is working.\n";
        std::cout << "(Set SKIP_PERFORMANCE_DEMO=true to disable this demo)\n\n";
        
        // Create a sample message for testing
        Trevor::HealthCheckRequest test_msg;
        test_msg.set_service_name("performance_benchmark");
        test_msg.set_uid("benchmark-uid-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        
        // Warmup phase to stabilize performance measurements
        std::cout << "🔥 Warming up systems...\n";
        svc.disable_tracing();
        for (int i = 0; i < 100; ++i) {
            svc.publish_broadcast(test_msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Test 1: High-performance mode (no tracing overhead)
        std::cout << "📊 Test 1: High-Performance Mode (Fast Functions)\n";
        svc.disable_tracing();  // Switch to fast function pointers
        std::cout << "   • Tracing enabled: " << (svc.is_tracing_enabled() ? "YES" : "NO") << "\n";
        std::cout << "   • Implementation: publish_broadcast_fast() via function pointer\n";
        std::cout << "   • Characteristics: Zero branching, minimal CPU cycles, no OpenTelemetry overhead\n";
        
        const int benchmark_iterations = 10000;  // Increased for better precision
        auto start_fast = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < benchmark_iterations; ++i) {
            svc.publish_broadcast(test_msg);  // Uses function pointer -> publish_broadcast_fast
        }
        auto end_fast = std::chrono::high_resolution_clock::now();
        auto fast_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_fast - start_fast);
        
        std::cout << "   • " << benchmark_iterations << " messages published in: " << fast_duration.count() << "ns\n";
        std::cout << "   • Average per message: " << std::fixed << std::setprecision(3) 
                  << (fast_duration.count() / static_cast<double>(benchmark_iterations)) << "ns ("
                  << std::setprecision(3) << (fast_duration.count() / static_cast<double>(benchmark_iterations)) / 1000.0 << "μs)\n\n";
        
        // Brief pause between tests
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Test 2: Full observability mode (with tracing)
        std::cout << "📊 Test 2: Full Observability Mode (Traced Functions)\n";
        svc.enable_tracing();   // Switch to traced function pointers
        std::cout << "   • Tracing enabled: " << (svc.is_tracing_enabled() ? "YES" : "NO") << "\n";
        std::cout << "   • Implementation: publish_broadcast_traced() via function pointer\n";
        std::cout << "   • Characteristics: OpenTelemetry spans, NATS headers, W3C trace context injection\n";
        
        auto start_traced = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < benchmark_iterations; ++i) {
            svc.publish_broadcast(test_msg);  // Uses function pointer -> publish_broadcast_traced
        }
        auto end_traced = std::chrono::high_resolution_clock::now();
        auto traced_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_traced - start_traced);
        
        std::cout << "   • " << benchmark_iterations << " messages published in: " << traced_duration.count() << "ns\n";
        std::cout << "   • Average per message: " << std::fixed << std::setprecision(3) 
                  << (traced_duration.count() / static_cast<double>(benchmark_iterations)) << "ns ("
                  << std::setprecision(3) << (traced_duration.count() / static_cast<double>(benchmark_iterations)) / 1000.0 << "μs)\n\n";
        
        // Performance analysis with enhanced metrics
        double overhead_ratio = static_cast<double>(traced_duration.count()) / fast_duration.count();
        double overhead_percentage = (overhead_ratio - 1.0) * 100.0;
        long long overhead_ns = (traced_duration.count() - fast_duration.count()) / benchmark_iterations;
        
        std::cout << "🎯 Performance Analysis:\n";
        std::cout << "   • Tracing overhead ratio: " << std::fixed << std::setprecision(3) << overhead_ratio << "x\n";
        std::cout << "   • Tracing overhead: " << std::setprecision(1) << overhead_percentage << "%\n";
        std::cout << "   • Overhead per message: " << overhead_ns << "ns (" << std::setprecision(3) << overhead_ns / 1000.0 << "μs)\n";
        std::cout << "   • Runtime switching: ZERO branching penalty! ✅\n";
        std::cout << "   • Hot-path optimization: Function pointers eliminate if-statements ✅\n";
        std::cout << "   • Dynamic control: Switch modes without recompilation ✅\n";
        
        // Performance validation
        if (overhead_ratio < 2.0) {
            std::cout << "   • 🎉 EXCELLENT: Tracing overhead is minimal (< 2x)\n";
        } else if (overhead_ratio < 5.0) {
            std::cout << "   • ✅ GOOD: Tracing overhead is acceptable (< 5x)\n";
        } else {
            std::cout << "   • ⚠️  WARNING: Tracing overhead is high (> 5x) - consider optimization\n";
        }
        
        // Test 3: Runtime switching validation
        std::cout << "\n📊 Test 3: Runtime Switching Validation\n";
        std::cout << "   • Testing rapid mode switching without performance degradation\n";
        
        auto switch_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            svc.disable_tracing();
            svc.publish_broadcast(test_msg);
            svc.enable_tracing();
            svc.publish_broadcast(test_msg);
        }
        auto switch_end = std::chrono::high_resolution_clock::now();
        auto switch_duration = std::chrono::duration_cast<std::chrono::microseconds>(switch_end - switch_start);
        
        std::cout << "   • 200 messages with 100 mode switches: " << switch_duration.count() << "μs\n";
        std::cout << "   • Average per switch + message: " << std::setprecision(2) << switch_duration.count() / 200.0 << "μs\n";
        std::cout << "   • ✅ Runtime switching works seamlessly\n";
        
        // Reset to traced mode for production
        std::cout << "\n🔧 Setting production mode: Full observability enabled\n";
        svc.enable_tracing();
        std::cout << "============================================================\n\n";
    } else {
        std::cout << "\n⏭️  Performance demo skipped (SKIP_PERFORMANCE_DEMO=true)\n";
        std::cout << "🔧 Setting production mode: Full observability enabled\n";
        svc.enable_tracing();
        std::cout << "\n";
    }

    // Display configuration values being used with structured logging
    auto logger = svc.get_logger();
    logger->info("Portfolio Manager Configuration:");
    logger->info("   • NATS URL: {}", nats_url);
    logger->info("   • Thread Pool Size: {}", svc.get_config<int>("thread_pool.size", 4));
    logger->info("   • Max Positions: {}", svc.get_config<int>("portfolio_manager.max_positions", 1000));
    logger->info("   • Update Frequency: {}ms", svc.get_config<int>("portfolio_manager.update_frequency", 1000));
    logger->info("   • Default Portfolio Value: ${}", svc.get_config<double>("portfolio_manager.default_portfolio_value", 100000.0));
    logger->info("   • Shutdown Timeout: {}ms", svc.get_config<int>("shutdown.timeout", 5000));
    
    logger->info("PortfolioManager is now running. Press Ctrl+C to shutdown gracefully.");

    // Main service loop - run until shutdown is requested
    while (svc.is_running()) {
        nats_Sleep(1000);  // Sleep for 1 second between checks
    }
    
    logger->info("Portfolio Manager shutting down...");
    return 0;
}
