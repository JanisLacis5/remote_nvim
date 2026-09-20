add_library(protocol INTERFACE)

target_link_libraries(protocol
    INTERFACE
        sanitizer_flags
)

target_include_directories(protocol
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
