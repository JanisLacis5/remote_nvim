#include "networking/socket.hpp"
#include "os/process.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <optional>
#include <sys/socket.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace
{

struct io_step {
    ssize_t result;
    int error{};
};

int scripted_fd{ -1 };
std::deque<io_step> reads{};
std::deque<io_step> writes{};
std::vector<std::byte> sent_bytes{};
bool fail_fork{ false };
bool interrupt_wait{ false };
int wait_calls{ 0 };

} // unnamed namespace

// Only this executable wraps syscalls. The queues specify successive results
// for scripted_fd; unrelated descriptors (including Google Test output) use libc.
extern "C" {
ssize_t __real_read(int, void*, size_t);
ssize_t __real_send(int, const void*, size_t, int);
pid_t __real_fork();
pid_t __real_waitpid(pid_t, int*, int);
}

extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    if (fd != scripted_fd || reads.empty())
        return __real_read(fd, buffer, size);

    const auto step = reads.front();
    reads.pop_front();

    if (step.result < 0) {
        errno = step.error;
        return -1;
    }

    const auto requested_count = static_cast<std::size_t>(step.result);
    const auto count = std::min(size, requested_count);
    std::memset(buffer, 42, count);

    return static_cast<ssize_t>(count);
}

extern "C" ssize_t __wrap_send(int fd, const void* buffer, size_t size, int flags) {
    if (fd != scripted_fd || writes.empty())
        return __real_send(fd, buffer, size, flags);

    const auto step = writes.front();
    writes.pop_front();

    if (step.result < 0) {
        errno = step.error;
        return -1;
    }

    const auto count = std::min(size, static_cast<size_t>(step.result));
    const auto* bytes = static_cast<const std::byte*>(buffer);
    sent_bytes.insert(sent_bytes.end(), bytes, bytes + count);

    return static_cast<ssize_t>(count);
}

extern "C" pid_t __wrap_fork() {
    if (fail_fork) {
        errno = EAGAIN;
        return -1;
    }

    return __real_fork();
}

extern "C" pid_t __wrap_waitpid(pid_t pid, int* status, int options) {
    ++wait_calls;

    if (interrupt_wait) {
        interrupt_wait = false;
        errno = EINTR;
        return -1;
    }

    return __real_waitpid(pid, status, options);
}

namespace
{
class SyscallIo : public testing::Test
{
protected:
    int peer{ -1 };
    std::optional<networking::socket> socket{};

    void SetUp() override {
        int fds[2]{};

        const auto socket_pair_result = ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds);

        ASSERT_EQ(socket_pair_result, 0);
        scripted_fd = fds[0];
        peer = fds[1];
        socket.emplace(fds[0]);

        ASSERT_TRUE(socket->is_valid());
    }

    void TearDown() override {
        scripted_fd = -1;
        reads.clear();
        writes.clear();
        sent_bytes.clear();

        socket.reset();
        if (peer >= 0)
            ::close(peer);
    }
};

TEST_F(SyscallIo, ReadRetriesEintrBeforeAndAfterPartialData) {
    reads = { { -1, EINTR }, { 2 }, { -1, EINTR }, { 3 } };

    const auto received_bytes = socket->read(5);

    const std::vector<std::byte> expected_bytes(5, std::byte{ 42 });

    EXPECT_EQ(received_bytes, expected_bytes);
    EXPECT_TRUE(reads.empty());
}

TEST_F(SyscallIo, ReadAggregatesSeveralShortReads) {
    reads = { { 1 }, { 2 }, { 3 } };

    const auto received_bytes = socket->read(6);

    const std::vector<std::byte> expected_bytes(6, std::byte{ 42 });

    EXPECT_EQ(received_bytes, expected_bytes);
    EXPECT_TRUE(reads.empty());
}

TEST_F(SyscallIo, ReadDiscardsPartialDataOnPermanentError) {
    reads = { { 2 }, { -1, EIO } };

    const auto received_bytes = socket->read(5);

    EXPECT_TRUE(received_bytes.empty());
    EXPECT_TRUE(reads.empty());
}

TEST_F(SyscallIo, WriteRetriesEintrAndAdvancesByActualBytesSent) {
    writes = { { -1, EINTR }, { 2 }, { -1, EINTR }, { 1 }, { 2 } };
    const std::vector payload{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 }, std::byte{ 4 }, std::byte{ 5 } };

    const auto bytes_written = socket->write_all(payload);

    EXPECT_EQ(bytes_written, payload.size());
    EXPECT_EQ(sent_bytes, payload);
    EXPECT_TRUE(writes.empty());
}

TEST_F(SyscallIo, WriteReturnsPartialCountAfterPermanentError) {
    writes = { { 2 }, { -1, EPIPE } };
    const std::vector payload{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };

    const auto bytes_written = socket->write_all(payload);

    EXPECT_EQ(bytes_written, 2u);
    const std::vector expected_prefix{ std::byte{ 1 }, std::byte{ 2 } };
    EXPECT_EQ(sent_bytes, expected_prefix);
}

TEST_F(SyscallIo, WriteStopsWhenSyscallMakesNoProgress) {
    writes = { { 2 }, { 0 } };
    const std::vector<std::byte> payload(5, std::byte{ 42 });

    const auto bytes_written = socket->write_all(payload);

    EXPECT_EQ(bytes_written, 2u);
    EXPECT_EQ(sent_bytes.size(), 2u);
}

TEST_F(SyscallIo, InitialWriteErrorReturnsZero) {
    writes = { { -1, EIO } };
    const std::vector<std::byte> payload(5, std::byte{ 42 });

    const auto bytes_written = socket->write_all(payload);

    EXPECT_EQ(bytes_written, 0u);
    EXPECT_TRUE(sent_bytes.empty());
}

TEST(ProcessSyscalls, ForkFailureThrowsSystemErrorWithoutWaitingForUnrelatedChildren) {
    fail_fork = true;
    wait_calls = 0;

    EXPECT_THROW((process{ "true" }), std::system_error);
    fail_fork = false;

    EXPECT_EQ(wait_calls, 0);
}

TEST(ProcessSyscalls, DestructorRetriesInterruptedWaitAndReapsChild) {
    wait_calls = 0;

    {
        process child{ "true" };
        interrupt_wait = true;
    }

    EXPECT_GE(wait_calls, 2);

    int status{ 0 };

    const auto remaining_child = __real_waitpid(-1, &status, WNOHANG);
    EXPECT_EQ(remaining_child, -1);
    if (remaining_child == -1)
        EXPECT_EQ(errno, ECHILD);

    // If the destructor failed to wait, reap the test's child before exiting.
    if (remaining_child == 0)
        while (__real_waitpid(-1, &status, 0) < 0 && errno == EINTR) {
        }
}

} // namespace
