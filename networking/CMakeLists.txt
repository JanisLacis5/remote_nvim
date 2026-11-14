add_library(networking
    src/listener.c
    ${PROJECT_SOURCE_DIR}/platform/posix/socket.c
)

target_link_libraries(networking
    PUBLIC
        sanitizer_flags
)

target_include_directories(networking
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
