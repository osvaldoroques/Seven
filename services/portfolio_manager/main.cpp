#include "portfolio_manager.hpp"
#include "messages.pb.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    std::string config_file = "config.yaml";
    
    // Allow config file to be specified as command line argument
    if (argc > 1) {
        config_file = argv[1];
    }

    try {
        // Create Portfolio Manager service instance
        PortfolioManager svc("portfolio-001", config_file);
        
        std::cout << "🚀 Starting Portfolio Manager service with config: " << config_file << std::endl;
        
        // Run the service with parallel initialization
        svc.run_parallel();
        
        std::cout << "✅ Portfolio Manager service shutdown complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
