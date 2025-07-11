#pragma once

#include "service_host.hpp"
#include <iostream>

class PortfolioManager : public ServiceHost {
public:
    // Constructor without config file
    template<typename... Regs>
    PortfolioManager(const std::string& uid, Regs&&... regs)
      : ServiceHost(uid, "PortfolioManager", std::forward<Regs>(regs)...)
    {
        std::cout << "✅ PortfolioManager initialized with UID: " << uid << std::endl;
    }
    
    // Constructor with config file - using a tag to disambiguate
    struct ConfigFileTag {};
    template<typename... Regs>
    PortfolioManager(ConfigFileTag, const std::string& uid, const std::string& config_file, Regs&&... regs)
      : ServiceHost(uid, "PortfolioManager", config_file, std::forward<Regs>(regs)...)
    {
        std::cout << "✅ PortfolioManager initialized with UID: " << uid << " and config: " << config_file << std::endl;
    }
};
