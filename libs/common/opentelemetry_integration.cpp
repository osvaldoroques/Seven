#include "opentelemetry_integration.hpp"
#include <iostream>
#include <cstdlib>

#ifdef HAVE_OPENTELEMETRY
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/trace/provider.h>

namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;
namespace trace_propagation = opentelemetry::trace::propagation;

std::shared_ptr<trace_api::Tracer> OpenTelemetryIntegration::tracer_ = nullptr;
std::unique_ptr<trace_propagation::HttpTraceContext> OpenTelemetryIntegration::propagator_ = nullptr;
bool OpenTelemetryIntegration::initialized_ = false;
#else
std::shared_ptr<trace_api::Tracer> OpenTelemetryIntegration::tracer_ = nullptr;
bool OpenTelemetryIntegration::initialized_ = false;
#endif

bool OpenTelemetryIntegration::initialize(const std::string& service_name, const std::string& otlp_endpoint) {
#ifdef HAVE_OPENTELEMETRY
    if (initialized_) {
        return true;
    }

    try {
        // Create OTLP exporter
        otlp::OtlpGrpcExporterOptions exporter_options;
        exporter_options.endpoint = otlp_endpoint;
        auto exporter = std::make_unique<otlp::OtlpGrpcExporter>(exporter_options);

        // Create span processor
        trace_sdk::BatchSpanProcessorOptions processor_options{};
        auto processor = std::make_unique<trace_sdk::BatchSpanProcessor>(std::move(exporter), processor_options);

        // Create resource
        auto resource_attributes = resource::ResourceAttributes{
            {"service.name", service_name},
            {"service.version", "1.0.0"}
        };
        auto resource = resource::Resource::Create(resource_attributes);

        // Create tracer provider
        auto provider = std::make_shared<trace_sdk::TracerProvider>(std::move(processor), resource);
        
        // Set global tracer provider
        trace_api::Provider::SetTracerProvider(provider);

        // Get tracer
        tracer_ = trace_api::Provider::GetTracerProvider()->GetTracer(service_name, "1.0.0");

        // Initialize propagator
        propagator_ = std::make_unique<trace_propagation::HttpTraceContext>();

        initialized_ = true;
        std::cout << "✅ OpenTelemetry initialized: " << service_name 
                  << " -> " << otlp_endpoint << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to initialize OpenTelemetry: " << e.what() << std::endl;
        return false;
    }
#else
    std::cout << "⚠️  OpenTelemetry not available (compiled without HAVE_OPENTELEMETRY)" << std::endl;
    return false;
#endif
}

std::shared_ptr<void> OpenTelemetryIntegration::get_tracer() {
#ifdef HAVE_OPENTELEMETRY
    return std::static_pointer_cast<void>(tracer_);
#else
    return nullptr;
#endif
}

std::shared_ptr<void> OpenTelemetryIntegration::start_span(const std::string& operation_name, 
                                                         const std::unordered_map<std::string, std::string>& context) {
#ifdef HAVE_OPENTELEMETRY
    if (!tracer_) {
        return nullptr;
    }

    trace_api::StartSpanOptions options;
    
    // If context is provided, extract parent context
    if (!context.empty() && propagator_) {
        auto mutable_context = const_cast<std::unordered_map<std::string, std::string>&>(context);
        TextMapCarrier carrier(mutable_context);
        auto ctx = propagator_->Extract(carrier, opentelemetry::context::Context{});
        options.parent = trace_api::GetSpan(ctx)->GetContext();
    }

    auto span = tracer_->StartSpan(operation_name, options);
    return std::static_pointer_cast<void>(span);
#else
    return nullptr;
#endif
}

std::shared_ptr<void> OpenTelemetryIntegration::start_child_span(const std::string& operation_name, 
                                                               std::shared_ptr<void> parent_span) {
#ifdef HAVE_OPENTELEMETRY
    if (!tracer_ || !parent_span) {
        return start_span(operation_name);
    }

    auto parent = std::static_pointer_cast<trace_api::Span>(parent_span);
    trace_api::StartSpanOptions options;
    options.parent = parent->GetContext();

    auto span = tracer_->StartSpan(operation_name, options);
    return std::static_pointer_cast<void>(span);
#else
    return nullptr;
#endif
}

void OpenTelemetryIntegration::end_span(std::shared_ptr<void> span) {
#ifdef HAVE_OPENTELEMETRY
    if (span) {
        auto otel_span = std::static_pointer_cast<trace_api::Span>(span);
        otel_span->End();
    }
#endif
}

void OpenTelemetryIntegration::add_span_attributes(std::shared_ptr<void> span, 
                                                 const std::unordered_map<std::string, std::string>& attributes) {
#ifdef HAVE_OPENTELEMETRY
    if (span) {
        auto otel_span = std::static_pointer_cast<trace_api::Span>(span);
        for (const auto& [key, value] : attributes) {
            otel_span->SetAttribute(key, value);
        }
    }
#endif
}

std::unordered_map<std::string, std::string> OpenTelemetryIntegration::extract_trace_context(
    const std::unordered_map<std::string, std::string>& headers) {
#ifdef HAVE_OPENTELEMETRY
    std::unordered_map<std::string, std::string> result;
    if (propagator_) {
        auto mutable_headers = const_cast<std::unordered_map<std::string, std::string>&>(headers);
        TextMapCarrier carrier(mutable_headers);
        auto ctx = propagator_->Extract(carrier, opentelemetry::context::Context{});
        
        // Extract trace context back to headers for internal use
        TextMapCarrier result_carrier(result);
        propagator_->Inject(result_carrier, ctx);
    }
    return result;
#else
    return {};
#endif
}

std::unordered_map<std::string, std::string> OpenTelemetryIntegration::inject_trace_context(
    std::shared_ptr<void> span) {
#ifdef HAVE_OPENTELEMETRY
    std::unordered_map<std::string, std::string> headers;
    if (propagator_) {
        auto ctx = opentelemetry::context::Context{};
        if (span) {
            auto otel_span = std::static_pointer_cast<trace_api::Span>(span);
            ctx = trace_api::SetSpan(ctx, otel_span);
        }
        
        TextMapCarrier carrier(headers);
        propagator_->Inject(carrier, ctx);
    }
    return headers;
#else
    return {};
#endif
}

std::pair<std::string, std::string> OpenTelemetryIntegration::get_trace_and_span_ids(std::shared_ptr<void> span) {
#ifdef HAVE_OPENTELEMETRY
    if (span) {
        auto otel_span = std::static_pointer_cast<trace_api::Span>(span);
        auto context = otel_span->GetContext();
        
        char trace_id[32];
        char span_id[16];
        context.trace_id().ToLowerBase16(trace_id);
        context.span_id().ToLowerBase16(span_id);
        
        return {std::string(trace_id, 32), std::string(span_id, 16)};
    }
    return {"", ""};
#else
    return {"", ""};
#endif
}
