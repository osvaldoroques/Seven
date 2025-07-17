add_library(common STATIC
    service_host_impl.cpp
    service_cache.cpp
    service_scheduler.cpp
    logger.cpp
    opentelemetry_integration.cpp
)

target_include_directories(common PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# Link to NATS, Protobuf, Threads
target_link_libraries(common
    PUBLIC
    proto_files
    nats
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

# Conditionally link yaml-cpp if available
if(YAMLCPP_FOUND)
    target_include_directories(common PUBLIC ${YAMLCPP_INCLUDE_DIRS})
    # Use the absolute path to the yaml-cpp library
    find_library(YAMLCPP_LIB yaml-cpp PATHS ${YAMLCPP_LIBRARY_DIRS} NO_DEFAULT_PATH)
    if(YAMLCPP_LIB)
        target_link_libraries(common PUBLIC ${YAMLCPP_LIB})
        message(STATUS "Linking common library with yaml-cpp support: ${YAMLCPP_LIB}")
    else()
        target_link_libraries(common PUBLIC ${YAMLCPP_LIBRARIES})
        message(STATUS "Linking common library with yaml-cpp support: ${YAMLCPP_LIBRARIES}")
    endif()
else()
    message(STATUS "Building common library with simplified configuration")
endif()

# Conditionally link spdlog if available
if(spdlog_FOUND AND fmt_FOUND)
    target_link_libraries(common PUBLIC spdlog::spdlog fmt::fmt)
    message(STATUS "Linking common library with spdlog structured logging")
else()
    message(STATUS "Building common library with stdout logging fallback")
endif()

# Try to find OpenTelemetry - manual detection first for better control
find_path(OPENTELEMETRY_INCLUDE_DIR 
    NAMES opentelemetry/version.h opentelemetry/api.h
    PATHS /usr/local/include /usr/include
    PATH_SUFFIXES opentelemetry
)

find_library(OPENTELEMETRY_API_LIB 
    NAMES opentelemetry_api libopentelemetry_api
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
)

find_library(OPENTELEMETRY_SDK_LIB 
    NAMES opentelemetry_trace libopentelemetry_trace
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
)

find_library(OPENTELEMETRY_OTLP_LIB 
    NAMES opentelemetry_otlp_grpc_exporter libopentelemetry_otlp_grpc_exporter
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
)

find_library(OPENTELEMETRY_COMMON_LIB 
    NAMES opentelemetry_common libopentelemetry_common
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
)

find_library(OPENTELEMETRY_RESOURCES_LIB 
    NAMES opentelemetry_resources libopentelemetry_resources
    PATHS /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu
)

# OpenTelemetry setup - try to find but allow fallback
if(OPENTELEMETRY_INCLUDE_DIR AND OPENTELEMETRY_API_LIB AND OPENTELEMETRY_SDK_LIB)
    target_include_directories(common PUBLIC ${OPENTELEMETRY_INCLUDE_DIR})
    
    # Link OpenTelemetry libraries
    set(OTEL_LIBS 
        ${OPENTELEMETRY_API_LIB}
        ${OPENTELEMETRY_SDK_LIB}
        ${OPENTELEMETRY_COMMON_LIB}
        ${OPENTELEMETRY_RESOURCES_LIB}
    )
    
    # Add OTLP exporter if available
    if(OPENTELEMETRY_OTLP_LIB)
        list(APPEND OTEL_LIBS ${OPENTELEMETRY_OTLP_LIB})
    endif()
    
    target_link_libraries(common PUBLIC ${OTEL_LIBS})
    target_compile_definitions(common PUBLIC HAVE_OPENTELEMETRY)
    message(STATUS "OpenTelemetry found - distributed tracing enabled")
    message(STATUS "OpenTelemetry include: ${OPENTELEMETRY_INCLUDE_DIR}")
    message(STATUS "OpenTelemetry libraries: ${OTEL_LIBS}")
else()
    message(WARNING "OpenTelemetry not found - using stub implementations for development")
    message(STATUS "Production builds require OpenTelemetry to be installed")
    target_compile_definitions(common PUBLIC OTEL_STUBS)
endif()
