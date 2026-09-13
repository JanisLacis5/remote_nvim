cmake_minimum_required(VERSION 3.28)

project(janisnvim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# library stuff is here for the future mainly
# add_library(janisnvim_core)
# target_sources(janisnvim_core
# PRIVATE
        # src/process.cpp
        # src/socket.cpp
        # src/protocol.cpp
        # )
# target_include_directories(janisnvim_core
    # PUBLIC
        # ${PROJECT_SOURCE_DIR}/include
# )
# target_compile_options(janisnvim_core
    # PRIVATE
        # -Wall
        # -Wextra
        # -Wpedantic
# )

add_executable(janisnvim
    src/remote.cpp
)

# target_link_libraries(janisnvim
    # PRIVATE
        # janisnvim_core
# )

add_executable(janisnvimdaemon
    src/passive.cpp
)

# target_link_libraries(janisnvimdaemon
    # PRIVATE
        # janisnvim_core
# )
