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
    ${CMAKE_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)
