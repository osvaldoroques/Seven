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
