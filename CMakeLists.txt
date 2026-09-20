cmake_minimum_required(VERSION 3.28)

project(janisnvim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PLATFORM "" CACHE STRING "Target platform: linux, mac, or windows")
set_property(CACHE PLATFORM PROPERTY STRINGS linux mac windows)

if(PLATFORM STREQUAL "")
    if(WIN32)
        set(PLATFORM "windows")
    elseif(APPLE)
        set(PLATFORM "mac")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(PLATFORM "linux")
    else()
        message(FATAL_ERROR
            "Could not infer the target platform. Set PLATFORM to linux, mac, or windows."
        )
    endif()
endif()

string(TOLOWER "${PLATFORM}" PLATFORM)
set(PLATFORM "${PLATFORM}" CACHE STRING "Target platform: linux, mac, or windows" FORCE)

if(NOT PLATFORM MATCHES "^(linux|mac|windows)$")
    message(FATAL_ERROR
        "Unknown PLATFORM '${PLATFORM}'. Expected linux, mac, or windows."
    )
endif()

message(STATUS "Target platform: ${PLATFORM}")

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
add_subdirectory(protocol)

add_executable(janisnvim
    src/server.cpp
)

target_link_libraries(janisnvim
    PRIVATE
        networking
        protocol
)

add_executable(janisnvimdaemon
    src/client.cpp
)

target_link_libraries(janisnvimdaemon
    PRIVATE
        networking
        protocol
)
