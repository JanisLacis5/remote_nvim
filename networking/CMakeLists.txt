if(PLATFORM STREQUAL "windows")
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/windows/socket.cpp
    )
else()
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/posix/socket.cpp
    )
endif()

add_library(networking
    src/socket.cpp
    src/listener.cpp
    ${PLATFORM_SOCKET_SOURCE}
)

target_link_libraries(networking
    PUBLIC
        sanitizer_flags
        protocol
)

target_include_directories(networking
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
