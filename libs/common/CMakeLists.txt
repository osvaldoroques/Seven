add_library(common STATIC
    service_host_impl.cpp
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
    target_link_libraries(common PUBLIC ${YAMLCPP_LIBRARIES})
    message(STATUS "Linking common library with yaml-cpp support")
else()
    message(STATUS "Building common library with simplified configuration")
endif()
