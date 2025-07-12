cmake_minimum_required(VERSION 3.16)
project(RoboAdvisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
include(CTest)

# Option to disable tests for production builds
option(ENABLE_TESTS "Enable building tests" ON)

# Enable testing only if tests are enabled and we have CTest
if(ENABLE_TESTS)
    enable_testing()
else()
    message(STATUS "Tests disabled")
endif()

# Find dependencies
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# Try to find Catch2, but make it optional for Docker builds
if(ENABLE_TESTS)
    find_package(Catch2 QUIET)
    if(NOT Catch2_FOUND)
        message(WARNING "Catch2 not found - tests will be disabled")
        set(ENABLE_TESTS OFF)
    else()
        message(STATUS "Catch2 found - tests enabled")
    endif()
else()
    message(STATUS "Tests explicitly disabled")
endif()

find_package(PkgConfig REQUIRED)

# Try to find spdlog and fmt for logging
find_package(spdlog QUIET)
find_package(fmt QUIET)

if(spdlog_FOUND AND fmt_FOUND)
    message(STATUS "spdlog and fmt found - enabling structured logging")
    add_compile_definitions(HAVE_SPDLOG)
else()
    message(WARNING "spdlog/fmt not found - logging will use simple stdout")
endif()

# Try to find yaml-cpp, but make it optional
pkg_check_modules(YAMLCPP yaml-cpp)

if(YAMLCPP_FOUND)
    message(STATUS "yaml-cpp found - using full configuration support")
    add_compile_definitions(HAVE_YAML_CPP)
    # Set library directories for linking
    link_directories(${YAMLCPP_LIBRARY_DIRS})
else()
    message(WARNING "yaml-cpp not found - using simplified configuration")
endif()

message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")

# Auto-generate Protobuf files
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(proto_files STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(proto_files PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(proto_files PUBLIC ${Protobuf_LIBRARIES})

# Add subdirectories
add_subdirectory(libs/common)
add_subdirectory(services/portfolio_manager)

# Only add tests if Catch2 is available
if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()
