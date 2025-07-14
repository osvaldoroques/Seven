add_executable(test_service_host
    test_service_host.cpp
)

target_link_libraries(test_service_host
    PRIVATE
    common
    proto_files
    Catch2::Catch2WithMain
)

target_include_directories(test_service_host
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

add_test(NAME service_host_test COMMAND test_service_host)

# Graceful shutdown tests
add_executable(test_graceful_shutdown
    test_graceful_shutdown.cpp
)

target_link_libraries(test_graceful_shutdown
    PRIVATE
    common
    proto_files
    Catch2::Catch2WithMain
    pthread
)

target_include_directories(test_graceful_shutdown
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

add_test(NAME graceful_shutdown_test COMMAND test_graceful_shutdown)

# ThreadPool tests
add_executable(test_thread_pool
    test_thread_pool.cpp
)

target_link_libraries(test_thread_pool
    PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(test_thread_pool
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME thread_pool_test COMMAND test_thread_pool)

# ThreadPool integration tests
add_executable(test_thread_pool_integration
    test_thread_pool_integration.cpp
)

target_link_libraries(test_thread_pool_integration
    PRIVATE
    common
    proto_files
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(test_thread_pool_integration
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

add_test(NAME thread_pool_integration_test COMMAND test_thread_pool_integration)

# ThreadPool sanitizer tests
add_executable(test_thread_pool_sanitizers
    test_thread_pool_sanitizers.cpp
)

target_link_libraries(test_thread_pool_sanitizers
    PRIVATE
    common
    GTest::gtest
    GTest::gtest_main
    pthread
)

target_include_directories(test_thread_pool_sanitizers
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME thread_pool_sanitizers_test COMMAND test_thread_pool_sanitizers)
