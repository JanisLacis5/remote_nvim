add_library(protocol
    src/message_handler.cpp
)

target_link_libraries(protocol
    PUBLIC
        networking
        sanitizer_flags
)

target_include_directories(protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
