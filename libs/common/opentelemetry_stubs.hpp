#pragma once

// Stub implementations for OpenTelemetry when not available in dev environment
// These provide no-op implementations that allow the code to compile and run
// Production builds should use real OpenTelemetry implementation

#ifdef OTEL_STUBS

#include <string>
#include <memory>
#include <unordered_map>

namespace opentelemetry {
namespace trace {

enum class StatusCode {
    kUnset = 0,
    kOk = 1,
    kError = 2
};

// Stub Span class
class Span {
public:
    void SetAttribute(const std::string& key, const std::string& value) {}
    void SetAttribute(const std::string& key, int64_t value) {}
    void SetAttribute(const std::string& key, double value) {}
    void SetAttribute(const std::string& key, bool value) {}
    void SetStatus(StatusCode code, const std::string& description = "") {}
    void End() {}
    void AddEvent(const std::string& name) {}
    void AddEvent(const std::string& name, const std::unordered_map<std::string, std::string>& attributes) {}
};

// Stub Tracer class
class Tracer {
public:
    std::shared_ptr<Span> StartSpan(const std::string& name) {
        return std::make_shared<Span>();
    }
};

// Stub TracerProvider class
class TracerProvider {
public:
    std::shared_ptr<Tracer> GetTracer(const std::string& name, const std::string& version = "") {
        return std::make_shared<Tracer>();
    }
    
    static std::shared_ptr<TracerProvider> GetTracerProvider() {
        static auto provider = std::make_shared<TracerProvider>();
        return provider;
    }
};

// Static Provider namespace
namespace Provider {
    inline std::shared_ptr<TracerProvider> GetTracerProvider() {
        return TracerProvider::GetTracerProvider();
    }
}

} // namespace trace

namespace context {
    
// Stub Context class
class Context {
public:
    static Context GetCurrent() { return Context{}; }
    
    template<typename T>
    T GetValue(const std::string& key) const { return T{}; }
    
    template<typename T>
    Context SetValue(const std::string& key, T value) const { return *this; }
};

// Stub Token class
class Token {
public:
    ~Token() = default;
};

// Stub scope functions
inline std::unique_ptr<Token> Attach(const Context& context) {
    return std::make_unique<Token>();
}

inline Context Detach(const Token& token) {
    return Context{};
}

} // namespace context

namespace propagation {

// Stub TextMapCarrier
template<typename T>
class TextMapCarrier {
public:
    virtual std::string Get(const std::string& key) const = 0;
    virtual void Set(const std::string& key, const std::string& value) = 0;
    virtual ~TextMapCarrier() = default;
};

// Stub TextMapPropagator
class TextMapPropagator {
public:
    template<typename T>
    context::Context Extract(const T& carrier, const context::Context& context) {
        return context;
    }
    
    template<typename T>
    void Inject(T& carrier, const context::Context& context) {
        // No-op
    }
};

// Stub GlobalTextMapPropagator
namespace GlobalTextMapPropagator {
    inline std::shared_ptr<TextMapPropagator> GetGlobalPropagator() {
        static auto propagator = std::make_shared<TextMapPropagator>();
        return propagator;
    }
}

} // namespace propagation
} // namespace opentelemetry

#endif // OTEL_STUBS
