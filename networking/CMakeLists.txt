add_library(networking
    src/socket.cpp
    src/listener.cpp
)

target_link_libraries(networking
    PUBLIC
        sanitizer_flags
)

target_include_directories(networking
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
