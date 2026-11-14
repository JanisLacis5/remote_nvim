include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
function(add_remote_nvim_tests target label)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE GTest::gtest_main protocol os)
    gtest_discover_tests(${target}
        DISCOVERY_MODE PRE_TEST
        NO_PRETTY_VALUES
        PROPERTIES LABELS "${label}" TIMEOUT 15
    )
endfunction()

# The current socket/process implementations use Linux-specific APIs.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND PLATFORM STREQUAL "linux")
    # Contract tests use real socket peers through the public handler API.
    add_remote_nvim_tests(protocol_tests integration protocol_test.cpp)
    find_package(Threads REQUIRED)
    target_link_libraries(protocol_tests PRIVATE Threads::Threads)
    add_remote_nvim_tests(socket_tests integration socket_test.cpp listener_test.cpp)
    target_link_libraries(socket_tests PRIVATE Threads::Threads)
    add_remote_nvim_tests(message_handler_tests integration message_handler_test.cpp)
    target_link_libraries(message_handler_tests PRIVATE Threads::Threads)
    add_remote_nvim_tests(process_tests integration process_test.cpp)
    add_executable(process_test_helper process_helper.cpp)
    add_dependencies(process_tests process_test_helper)
    target_compile_definitions(process_tests PRIVATE
        PROCESS_TEST_HELPER="$<TARGET_FILE:process_test_helper>"
    )

    add_remote_nvim_tests(workspace_tests unit workspace_test.cpp)
    target_link_libraries(workspace_tests PRIVATE workspace)

    # wrappers for syscalls
    add_remote_nvim_tests(syscall_tests unit syscall_test.cpp)
    target_link_options(syscall_tests PRIVATE
        -Wl,--wrap=read -Wl,--wrap=send -Wl,--wrap=fork -Wl,--wrap=waitpid
    )
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(NVIM_EXECUTABLE nvim)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND Python3_FOUND AND NVIM_EXECUTABLE)
    add_test(NAME FileWorkflow COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/file_workflow_test.py
        $<TARGET_FILE:janisnvim> $<TARGET_FILE:janisnvimdaemon> ${NVIM_EXECUTABLE})
    set_tests_properties(FileWorkflow PROPERTIES LABELS integration TIMEOUT 90 RUN_SERIAL TRUE)
endif()
