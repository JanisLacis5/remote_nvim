cmake_minimum_required(VERSION 3.28)

project(janisnvim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SANITIZER "none" CACHE STRING "Sanitizer for Debug builds: none, address, thread")
set_property(CACHE SANITIZER PROPERTY STRINGS none address thread)

add_library(sanitizer_flags INTERFACE)

if(SANITIZER STREQUAL "address")
    target_compile_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )

    target_link_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address>
    )

elseif(SANITIZER STREQUAL "thread")
    target_compile_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=thread>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )

    target_link_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=thread>
    )

elseif(NOT SANITIZER STREQUAL "none")
    message(FATAL_ERROR "Unknown sanitizer: ${SANITIZER}")
endif()

add_subdirectory(networking)

add_executable(janisnvim
    src/remote.cpp
)

target_link_libraries(janisnvim
    PRIVATE
        networking
)

add_executable(janisnvimdaemon
    src/passive.cpp
)

target_link_libraries(janisnvimdaemon
    PRIVATE
        networking
)
