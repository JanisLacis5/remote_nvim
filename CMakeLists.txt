cmake_minimum_required(VERSION 3.28)

project(janisnvim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
