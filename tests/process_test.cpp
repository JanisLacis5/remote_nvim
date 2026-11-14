#include "os/process.hpp"
#include "test_support.hpp"
#include <fcntl.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <type_traits>

namespace
{
using namespace test_support;

TEST(Process, IsMoveOnly) {
    EXPECT_FALSE(std::is_copy_constructible_v<process>);
    EXPECT_FALSE(std::is_copy_assignable_v<process>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<process>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<process>);
}

TEST(Process, RejectsAnEmptyArgumentList) {
    const std::initializer_list<std::string_view> empty_arguments{};

    EXPECT_THROW({ process child{ empty_arguments }; }, std::invalid_argument);
}

TEST(Process, RejectsAnEmptyExecutable) {
    EXPECT_THROW({ process child{ "" }; }, std::invalid_argument);
}

TEST(Process, DestructorWaitsForChildAndReapsIt) {
    socket_pair pair{};

    const auto descriptor_result = ::fcntl(pair.local.get(), F_SETFD, 0);

    ASSERT_EQ(descriptor_result, 0);

    {
        const auto child_fd = pair.local.get();
        const auto child_fd_argument = std::to_string(child_fd);
        process child{ PROCESS_TEST_HELPER, "write", child_fd_argument, "finished" };
    }

    const auto expected_bytes = vector_byte({ 'f', 'i', 'n', 'i', 's', 'h', 'e', 'd', '\n' });
    const auto received_bytes = read_exact_bytes(pair.peer.get(), 9);

    EXPECT_EQ(received_bytes, expected_bytes);

    int status{ 0 };

    const auto wait_result = ::waitpid(-1, &status, WNOHANG);

    EXPECT_EQ(wait_result, -1);
    EXPECT_EQ(errno, ECHILD);
}

TEST(Process, PassesSpacesEmptyArgumentsAndShellCharactersLiterally) {
    socket_pair pair{};

    const auto descriptor_result = ::fcntl(pair.local.get(), F_SETFD, 0);

    ASSERT_EQ(descriptor_result, 0);

    {
        const auto child_fd = pair.local.get();
        const auto child_fd_argument = std::to_string(child_fd);
        process child{ PROCESS_TEST_HELPER, "write", child_fd_argument, "a b", "", "$HOME;*" };
    }

    const auto expected_bytes = vector_byte({ 'a', ' ', 'b', '\n', '\n', '$', 'H', 'O', 'M', 'E', ';', '*', '\n' });
    const auto received_bytes = read_exact_bytes(pair.peer.get(), 13);

    EXPECT_EQ(received_bytes, expected_bytes);
}

TEST(Process, ReportsInheritedWorkingDirectoryWhileChildIsAlive) {
    socket_pair pair{};
    const auto descriptor_result = ::fcntl(pair.local.get(), F_SETFD, 0);

    ASSERT_EQ(descriptor_result, 0);

    const auto child_fd = pair.local.get();
    const auto child_fd_argument = std::to_string(child_fd);
    process child{ PROCESS_TEST_HELPER, "wait", child_fd_argument, "." };
    // Declared after child: closing this peer lets the child exit even if an
    // assertion returns early, before the process destructor waits for it.
    unique_fd parent_peer{ pair.peer.release() };
    pair.local.reset();

    const auto expected_bytes = vector_byte({ 'R' });
    const auto received_bytes = read_exact_bytes(parent_peer.get(), 1);

    ASSERT_EQ(received_bytes, expected_bytes);
    const auto expected_directory = std::filesystem::current_path();
    const auto child_directory = child.cwd();

    EXPECT_EQ(child_directory, expected_directory);

    const auto shutdown_result = ::shutdown(parent_peer.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);
}

TEST(Process, ReportsChildDirectoryInsteadOfParentDirectory) {
    socket_pair pair{};
    const auto descriptor_result = ::fcntl(pair.local.get(), F_SETFD, 0);

    ASSERT_EQ(descriptor_result, 0);

    const auto child_fd = pair.local.get();
    const auto child_fd_argument = std::to_string(child_fd);
    process child{ PROCESS_TEST_HELPER, "wait", child_fd_argument, "/" };
    // Declared after child: closing this peer lets the child exit even if an
    // assertion returns early, before the process destructor waits for it.
    unique_fd parent_peer{ pair.peer.release() };
    pair.local.reset();

    const auto expected_bytes = vector_byte({ 'R' });
    const auto received_bytes = read_exact_bytes(parent_peer.get(), 1);

    ASSERT_EQ(received_bytes, expected_bytes);
    const std::filesystem::path expected_directory{ "/" };
    const auto child_directory = child.cwd();

    EXPECT_EQ(child_directory, expected_directory);

    const auto shutdown_result = ::shutdown(parent_peer.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);
}

TEST(Process, FailedExecNeverReturnsIntoTheChildCaller) {
    socket_pair pair{};
    const auto parent_pid = ::getpid();

    {
        process child{ "/this/path/does/not/exist/remote-nvim-test" };
        if (::getpid() != parent_pid) {
            const char marker{ 'F' };

            ::write(pair.local.get(), &marker, 1);

            ::_exit(0);
        }
    }

    char marker{ '\0' };

    const auto received_count = ::recv(pair.peer.get(), &marker, 1, MSG_DONTWAIT);

    EXPECT_EQ(received_count, -1);
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);
}

TEST(Process, NonzeroChildExitDoesNotThrowFromDestructor) {
    const std::initializer_list<std::string_view> arguments{ PROCESS_TEST_HELPER, "exit", "7" };

    EXPECT_NO_THROW({ process child{ arguments }; });
}

TEST(Process, ResolvesAnExecutableUsingPath) {
    EXPECT_NO_THROW({ process child{ "true" }; });
}

} // namespace
