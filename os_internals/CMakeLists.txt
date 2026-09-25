if(PLATFORM STREQUAL "windows")
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/windows/process.cpp
    )
else()
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/posix/process.cpp
    )
endif()

add_library(os
    src/process.cpp
    ${PLATFORM_SOCKET_SOURCE}
)

target_link_libraries(os
    PUBLIC
        sanitizer_flags
        protocol
)

target_include_directories(os
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
