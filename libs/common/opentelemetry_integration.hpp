#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#ifdef HAVE_OPENTELEMETRY
#include <opentelemetry/api.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>

namespace otlp = opentelemetry::exporter::otlp;
namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace resource = opentelemetry::sdk::resource;
namespace propagation = opentelemetry::context::propagation;
namespace trace_propagation = opentelemetry::trace::propagation;
#endif

/**
 * OpenTelemetry integration for distributed tracing
 * Handles W3C Trace-Context propagation and OTLP export
 */
class OpenTelemetryIntegration {
private:
#ifdef HAVE_OPENTELEMETRY
    static std::shared_ptr<trace_api::Tracer> tracer_;
    static std::unique_ptr<trace_propagation::HttpTraceContext> propagator_;
    static bool initialized_;
#endif

public:
    /**
     * Initialize OpenTelemetry with OTLP exporter
     */
    static void initialize(const std::string& service_name, const std::string& otlp_endpoint = "http://localhost:4317");

    /**
     * Get the global tracer instance
     */
    static std::shared_ptr<void> get_tracer();

    /**
     * Start a new span
     */
    static std::shared_ptr<void> start_span(const std::string& operation_name, 
                                           const std::unordered_map<std::string, std::string>& context = {});

    /**
     * Start a child span with parent context
     */
    static std::shared_ptr<void> start_child_span(const std::string& operation_name, 
                                                 std::shared_ptr<void> parent_span);

    /**
     * End a span
     */
    static void end_span(std::shared_ptr<void> span);

    /**
     * Add attributes to a span
     */
    static void add_span_attributes(std::shared_ptr<void> span, 
                                  const std::unordered_map<std::string, std::string>& attributes);

    /**
     * Extract W3C Trace-Context from headers
     */
    static std::unordered_map<std::string, std::string> extract_trace_context(
        const std::unordered_map<std::string, std::string>& headers);

    /**
     * Inject W3C Trace-Context into headers
     */
    static std::unordered_map<std::string, std::string> inject_trace_context(
        std::shared_ptr<void> span = nullptr);

    /**
     * Get trace and span IDs from current context
     */
    static std::pair<std::string, std::string> get_trace_and_span_ids(std::shared_ptr<void> span = nullptr);

    /**
     * Shutdown OpenTelemetry
     */
    static void shutdown();

private:
#ifdef HAVE_OPENTELEMETRY
    // Custom text map carrier for propagation
    class TextMapCarrier : public propagation::TextMapCarrier {
    public:
        explicit TextMapCarrier(std::unordered_map<std::string, std::string>& headers) : headers_(headers) {}

        opentelemetry::nostd::string_view Get(opentelemetry::nostd::string_view key) const noexcept override {
            auto it = headers_.find(std::string(key));
            return it != headers_.end() ? opentelemetry::nostd::string_view(it->second) : "";
        }

        void Set(opentelemetry::nostd::string_view key, opentelemetry::nostd::string_view value) noexcept override {
            headers_[std::string(key)] = std::string(value);
        }

    private:
        std::unordered_map<std::string, std::string>& headers_;
    };
#endif
};

/**
 * RAII Span wrapper for automatic span management
 */
class TraceSpan {
private:
    std::shared_ptr<void> span_;
    std::string operation_name_;

public:
    explicit TraceSpan(const std::string& operation_name, 
                      const std::unordered_map<std::string, std::string>& context = {})
        : operation_name_(operation_name) {
        span_ = OpenTelemetryIntegration::start_span(operation_name, context);
    }

    explicit TraceSpan(const std::string& operation_name, std::shared_ptr<void> parent_span)
        : operation_name_(operation_name) {
        span_ = OpenTelemetryIntegration::start_child_span(operation_name, parent_span);
    }

    ~TraceSpan() {
        if (span_) {
            OpenTelemetryIntegration::end_span(span_);
        }
    }

    // Non-copyable, moveable
    TraceSpan(const TraceSpan&) = delete;
    TraceSpan& operator=(const TraceSpan&) = delete;
    TraceSpan(TraceSpan&& other) noexcept : span_(std::move(other.span_)), operation_name_(std::move(other.operation_name_)) {
        other.span_ = nullptr;
    }
    TraceSpan& operator=(TraceSpan&& other) noexcept {
        if (this != &other) {
            if (span_) {
                OpenTelemetryIntegration::end_span(span_);
            }
            span_ = std::move(other.span_);
            operation_name_ = std::move(other.operation_name_);
            other.span_ = nullptr;
        }
        return *this;
    }

    void add_attribute(const std::string& key, const std::string& value) {
        if (span_) {
            OpenTelemetryIntegration::add_span_attributes(span_, {{key, value}});
        }
    }

    void add_attributes(const std::unordered_map<std::string, std::string>& attributes) {
        if (span_) {
            OpenTelemetryIntegration::add_span_attributes(span_, attributes);
        }
    }

    std::shared_ptr<void> get_span() const { return span_; }

    std::pair<std::string, std::string> get_trace_and_span_ids() const {
        return OpenTelemetryIntegration::get_trace_and_span_ids(span_);
    }
};

// Convenience macros
#define TRACE_SPAN(operation_name) TraceSpan _trace_span(operation_name)
#define TRACE_SPAN_WITH_CONTEXT(operation_name, context) TraceSpan _trace_span(operation_name, context)
#define TRACE_CHILD_SPAN(operation_name, parent) TraceSpan _trace_span(operation_name, parent)
