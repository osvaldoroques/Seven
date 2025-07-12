add_library(common STATIC
    service_host_impl.cpp
    logger.cpp
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
