# 🚀 Portfolio Manager Service - Async Implementation with Permanent Task System
# This is the latest and most comprehensive implementation featuring:
# - Async infrastructure startup with parallel initialization
# - ServiceHost permanent task system for automatic monitoring
# - Clean separation of concerns (no redundant callbacks)
# - Production-ready configuration and error handling

add_executable(portfolio_manager
    main.cpp
    portfolio_manager.hpp
)

target_link_libraries(portfolio_manager
    PRIVATE
    common
    proto_files
    ${Protobuf_LIBRARIES}
)

target_include_directories(portfolio_manager
    PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/common
    ${CMAKE_SOURCE_DIR}/services/portfolio_manager
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
