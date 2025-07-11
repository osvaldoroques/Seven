#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "service_host.hpp"
#include "messages.pb.h"
#include <chrono>
#include <thread>

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
