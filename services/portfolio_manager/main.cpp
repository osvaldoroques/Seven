#include "portfolio_manager.hpp"
#include "messages_register.hpp"
#include "messages.pb.h"
#include <iostream>
#include <sstream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string config_file = "config.yaml";
    
    // Allow config file to be specified as command line argument
    if (argc > 1) {
        config_file = argv[1];
    }

    // Set up logging level from environment
    Logger::set_level_from_env();
    
    // Create main logger with tracing
    Logger main_logger("PortfolioManager.Main");
    main_logger.info("Starting Portfolio Manager with config: {} trace_id={} span_id={}", 
                     config_file, main_logger.get_trace_id(), main_logger.get_span_id());
    
    std::cout << "🚀 Starting Portfolio Manager with config: " << config_file << std::endl;
    
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
