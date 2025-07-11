#pragma once

#include "service_host.hpp"
#include <functional>

// A simple value type for one message registration
template<typename T>
struct MessageRegistration {
    MessageRouting routing;
    std::function<void(const T&)> handler;

    void Register(ServiceHost* host) const {
        host->register_message<T>(routing, handler);
    }
};

// Macro for concise declarations
#define MSG_REG(TYPE, ROUTING, HANDLER) \
    MessageRegistration<TYPE>{ROUTING, HANDLER}
