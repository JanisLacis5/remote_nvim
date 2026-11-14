if(PLATFORM STREQUAL "windows")
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/windows/process.c
    )
else()
    set(PLATFORM_SOCKET_SOURCE
        ${PROJECT_SOURCE_DIR}/platform/posix/process.c
    )
endif()

add_library(os
    ${PLATFORM_SOCKET_SOURCE}
)

target_link_libraries(os
    PUBLIC
        sanitizer_flags
)

target_include_directories(os
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
