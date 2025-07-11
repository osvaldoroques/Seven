#include "portfolio_manager.hpp"
#include "messages_register.hpp"
#include "messages.pb.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::string config_file = "config.yaml";
    
    // Allow config file to be specified as command line argument
    if (argc > 1) {
        config_file = argv[1];
    }
    
    std::cout << "🚀 Starting Portfolio Manager with config: " << config_file << std::endl;
    
    PortfolioManager svc(
      PortfolioManager::ConfigFileTag{},
      "svc-portfolio-001",
      config_file,
      MSG_REG(Trevor::HealthCheckRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::HealthCheckRequest& req) {
            std::cout << "✅ Received HealthCheckRequest from: "
                      << req.service_name()
                      << " UID: " << req.uid() << std::endl;

            Trevor::HealthCheckResponse res;
            res.set_service_name("PortfolioManager");
            res.set_uid(svc.get_uid());
            res.set_status(svc.get_status());  // Use enhanced status
            svc.publish_point_to_point(req.uid(), res);

            std::cout << "✅ Sent HealthCheckResponse with status: " << svc.get_status() << std::endl;
        }),
      MSG_REG(Trevor::PortfolioRequest, MessageRouting::PointToPoint,
        [&svc](const Trevor::PortfolioRequest& req) {
            std::cout << "📊 Processing PortfolioRequest for account: " << req.account_id() << std::endl;
            
            Trevor::PortfolioResponse res;
            res.set_account_id(req.account_id());
            res.set_total_value(svc.get_config<double>("portfolio_manager.default_portfolio_value", 100000.0));
            res.set_cash_balance(25000.0);
            res.set_status("active");
            
            // Add some sample positions from config
            auto max_positions = svc.get_config<int>("portfolio_manager.max_positions", 1000);
            std::cout << "📈 Portfolio supports up to " << max_positions << " positions" << std::endl;
            
            svc.publish_point_to_point(req.requester_uid(), res);
            std::cout << "✅ Sent PortfolioResponse for account: " << req.account_id() << std::endl;
        }),
      MSG_REG(Trevor::MarketDataUpdate, MessageRouting::Broadcast,
        [&svc](const Trevor::MarketDataUpdate& update) {
            std::cout << "📈 Market Data Update - Symbol: " << update.symbol() 
                      << ", Price: $" << update.price() 
                      << ", Volume: " << update.volume() << std::endl;
            
            // Process market data update based on configuration
            auto update_frequency = svc.get_config<int>("portfolio_manager.update_frequency", 1000);
            if (update_frequency > 0) {
                // Submit portfolio recalculation to thread pool
                svc.submit_task([symbol = update.symbol(), price = update.price()]() {
                    std::cout << "🔄 Recalculating portfolio for " << symbol 
                              << " at price $" << price << " in thread " 
                              << std::this_thread::get_id() << std::endl;
                    
                    // Simulate portfolio calculation work
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    std::cout << "✅ Portfolio calculation completed for " << symbol << std::endl;
                });
            }
        })
      // Additional message handlers can be added here as needed
    );

    svc.init_nats();
    svc.init_jetstream();
    
    // Display configuration values being used
    std::cout << "📋 Portfolio Manager Configuration:" << std::endl;
    std::cout << "   • NATS URL: " << svc.get_config<std::string>("nats.url", "nats://localhost:4222") << std::endl;
    std::cout << "   • Thread Pool Size: " << svc.get_config<int>("thread_pool.size", 4) << std::endl;
    std::cout << "   • Max Positions: " << svc.get_config<int>("portfolio_manager.max_positions", 1000) << std::endl;
    std::cout << "   • Update Frequency: " << svc.get_config<int>("portfolio_manager.update_frequency", 1000) << "ms" << std::endl;
    std::cout << "   • Default Portfolio Value: $" << svc.get_config<double>("portfolio_manager.default_portfolio_value", 100000.0) << std::endl;
    std::cout << "   • Shutdown Timeout: " << svc.get_config<int>("shutdown.timeout", 5000) << "ms" << std::endl;
    
    std::cout << "✅ PortfolioManager is now running. Press Ctrl+C to shutdown gracefully." << std::endl;

    // Main service loop - run until shutdown is requested
    while (svc.is_running()) {
        nats_Sleep(1000);  // Sleep for 1 second between checks
    }
    
    std::cout << "🛑 Portfolio Manager shutting down..." << std::endl;
    return 0;
}
// and functionality as needed for your portfolio management service.
