add_library(protocol
    src/proto.cpp
)

target_link_libraries(protocol
    PUBLIC
        sanitizer_flags
)

target_include_directories(protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
