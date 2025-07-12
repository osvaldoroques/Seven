#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "service_host.hpp"
#include "logger.hpp"
#include "messages.pb.h"
#include <chrono>
#include <thread>
#include <fstream>

TEST_CASE("ServiceHost handles HealthCheckRequest", "[ServiceHost]") {
    ServiceHost svc("test-uid", "TestService");

    std::atomic<bool> called{false};
    svc.register_message<Trevor::HealthCheckRequest>(
        MessageRouting::Broadcast,
        [&called](const Trevor::HealthCheckRequest& req) {
            called = true;
            REQUIRE(req.service_name() == "UnitTestCaller");
        }
    );

    Trevor::HealthCheckRequest req;
    req.set_service_name("UnitTestCaller");
    req.set_uid("test-caller-uid");

    std::string serialized;
    req.SerializeToString(&serialized);

    svc.receive_message(Trevor::HealthCheckRequest::descriptor()->full_name(), serialized);
    
    // Wait for thread pool task to complete
    auto start = std::chrono::steady_clock::now();
    while (!called && 
           std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    REQUIRE(called);
}

TEST_CASE("Logger creates correlation IDs and handles daily rotation", "[Logger]") {
    Logger logger("TestService");
    
    // Test correlation ID generation
    auto correlation_id = logger.get_correlation_id();
    REQUIRE(correlation_id.length() == 8);
    
    // Test child logger inherits correlation ID
    auto child = logger.create_child("Component");
    REQUIRE(child->get_correlation_id() == correlation_id);
    
    // Test request logger gets new correlation ID
    auto request_logger = logger.create_request_logger();
    REQUIRE(request_logger->get_correlation_id() != correlation_id);
    REQUIRE(request_logger->get_correlation_id().length() == 8);
    
    // Test logging methods don't crash
    logger.info("Test message: {}", 42);
    logger.warn("Warning with string: {}", "test");
    logger.error("Error test");
}

TEST_CASE("Logger supports distributed tracing with trace_id and span_id", "[Logger]") {
    Logger logger("TestService");
    
    // Test trace_id and span_id generation
    auto trace_id = logger.get_trace_id();
    auto span_id = logger.get_span_id();
    REQUIRE(trace_id.length() == 16);  // OpenTelemetry standard
    REQUIRE(span_id.length() == 8);   // OpenTelemetry standard
    
    // Test child logger inherits trace_id but gets new span_id
    auto child = logger.create_child("Database");
    REQUIRE(child->get_trace_id() == trace_id);        // Same trace
    REQUIRE(child->get_span_id() != span_id);          // New span
    REQUIRE(child->get_span_id().length() == 8);
    
    // Test span logger inherits trace_id but gets new span_id
    auto span_logger = logger.create_span_logger("HTTP Request");
    REQUIRE(span_logger->get_trace_id() == trace_id);   // Same trace
    REQUIRE(span_logger->get_span_id() != span_id);     // New span
    REQUIRE(span_logger->get_span_id().length() == 8);
    
    // Test request logger gets new trace_id and span_id
    auto request_logger = logger.create_request_logger();
    REQUIRE(request_logger->get_trace_id() != trace_id);  // New trace
    REQUIRE(request_logger->get_span_id() != span_id);    // New span
    REQUIRE(request_logger->get_trace_id().length() == 16);
    REQUIRE(request_logger->get_span_id().length() == 8);
    
    // Test structured logging with trace information
    logger.info("Processing user request: user_id={}", 12345);
    child->debug("Database query executed: duration={}ms", 25);
    span_logger->warn("High latency detected: {}ms", 150);
}
