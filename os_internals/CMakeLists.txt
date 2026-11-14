add_library(os
    ${PROJECT_SOURCE_DIR}/platform/posix/process.c
)

target_link_libraries(os
    PUBLIC
        sanitizer_flags
)

target_include_directories(os
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
