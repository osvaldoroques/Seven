cmake_minimum_required(VERSION 3.16)
project(RoboAdvisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer options (enabled in Debug builds by default)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)

# Auto-enable sanitizers in Debug mode (can be overridden)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT DEFINED ENABLE_ASAN AND NOT DEFINED ENABLE_UBSAN)
    set(ENABLE_ASAN ON)
    set(ENABLE_UBSAN ON)
    message(STATUS "Debug build detected - enabling AddressSanitizer and UBSan by default")
endif()

# Configure sanitizer flags
set(SANITIZER_FLAGS "")
set(SANITIZER_LINK_FLAGS "")

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    list(APPEND SANITIZER_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer")
    list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=address")
endif()

if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    list(APPEND SANITIZER_FLAGS "-fsanitize=undefined" "-fno-sanitize-recover=undefined")
    list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=undefined")
endif()

if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    list(APPEND SANITIZER_FLAGS "-fsanitize=thread" "-fno-omit-frame-pointer")
    list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=thread")
endif()

if(ENABLE_MSAN)
    message(STATUS "MemorySanitizer enabled")
    list(APPEND SANITIZER_FLAGS "-fsanitize=memory" "-fno-omit-frame-pointer")
    list(APPEND SANITIZER_LINK_FLAGS "-fsanitize=memory")
endif()

# Apply sanitizer flags globally
if(SANITIZER_FLAGS)
    string(REPLACE ";" " " SANITIZER_FLAGS_STR "${SANITIZER_FLAGS}")
    string(REPLACE ";" " " SANITIZER_LINK_FLAGS_STR "${SANITIZER_LINK_FLAGS}")
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS_STR}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS_STR}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS_STR}")
    
    message(STATUS "Sanitizer flags applied: ${SANITIZER_FLAGS_STR}")
endif()

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

# Find required dependencies (with fallback support for development)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# Add header-only LRU cache library
include(FetchContent)
# Note: Using internal LRU cache implementation instead of external dependency
# This provides better control and avoids build issues with external repositories

message(STATUS "spdlog and fmt found - structured logging enabled")

# Find yaml-cpp (now required)
pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)
message(STATUS "yaml-cpp found - full configuration support enabled")
# Set library directories for linking
link_directories(${YAMLCPP_LIBRARY_DIRS})

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
