cmake_minimum_required(VERSION 3.28)

project(janisnvim LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(_GNU_SOURCE)

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

if(SANITIZER MATCHES "^(address|thread)$")
    target_compile_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=${SANITIZER}>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )

    target_link_options(sanitizer_flags INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=${SANITIZER}>
    )

elseif(NOT SANITIZER STREQUAL "none")
    message(FATAL_ERROR "Unknown sanitizer: ${SANITIZER}")
endif()

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-format")
    message(FATAL_ERROR "No .clang-format file found in ${CMAKE_SOURCE_DIR}")
endif ()

set(CLANG_FORMAT_BIN "/usr/bin/clang-format-20")
file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/*.hpp"
)
list(FILTER FORMAT_FILES EXCLUDE REGEX "^${PROJECT_SOURCE_DIR}/build/")

add_custom_target(format
    COMMAND ${CLANG_FORMAT_BIN} -i ${FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running ${CLANG_FORMAT_BIN}"
)

add_subdirectory(networking)
add_subdirectory(protocol)
add_subdirectory(os_internals)

add_library(workspace src/workspace.cpp)
target_include_directories(workspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(workspace PUBLIC protocol)
configure_file(nvim/sync.lua ${CMAKE_CURRENT_BINARY_DIR}/sync.lua COPYONLY)

add_executable(janisnvim
    src/server.cpp
)

target_link_libraries(janisnvim
    PRIVATE
        networking
        protocol
        os
        workspace
)

add_executable(janisnvimdaemon
    src/client.cpp
)

target_link_libraries(janisnvimdaemon
    PRIVATE
        networking
        protocol
        os
        workspace
)

target_compile_definitions(janisnvimdaemon PRIVATE JANISNVIM_LUA="${CMAKE_CURRENT_BINARY_DIR}/sync.lua")

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
