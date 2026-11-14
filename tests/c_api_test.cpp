#include "os/process.h"
#include "session.h"
#include <cerrno>
#include <cstdlib>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

TEST(CApi, TruncatedFrameReleasesPreviouslyReadFields) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

    const unsigned char bytes[] = { 'J', 'N', 1, MSG_SAVE, 2, 0, 0, 0, 0, 1, 'x', 0, 0, 0, 2, 'y' };
    EXPECT_EQ(net_write(fds[1], bytes, sizeof(bytes)), sizeof(bytes));
    shutdown(fds[1], SHUT_WR);

    struct message value{};
    EXPECT_FALSE(message_read(fds[0], &value));
    EXPECT_EQ(value.count, 0U);
    EXPECT_EQ(value.fields[0].data, nullptr);
    message_clear(&value);
    message_clear(&value);

    close(fds[0]);
    close(fds[1]);
}

TEST(CApi, TextFieldsRejectEmbeddedNullButFileContentsRemainBinary) {
    char data[] = { 'a', '\0', 'b', '\0' };
    EXPECT_FALSE(field_is_text({ data, 3 }));
    EXPECT_TRUE(field_is_text({ data, 1 }));
    EXPECT_FALSE(field_is_text({ nullptr, 0 }));
}

TEST(CApi, FailedReadLeavesAnEmptySnapshotAndAnError) {
    struct snapshot value{};
    const char* error = nullptr;

    EXPECT_FALSE(ws_read("/proc/self/no-such-janisnvim-file", &value, &error));
    EXPECT_NE(error, nullptr);
    EXPECT_EQ(value.content.data, nullptr);
    EXPECT_EQ(value.content.size, 0U);
    ws_snapshot_clear(&value);
}

TEST(CApi, ChildExitStatusSurvivesPollingAndRepeatedWaits) {
    struct child_process child{};
    char executable[] = "sh", option[] = "-c", command[] = "exit 7";
    char* argv[] = { executable, option, command, nullptr };
    ASSERT_TRUE(process_spawn(&child, argv));

    EXPECT_EQ(process_wait(&child), 7);
    EXPECT_FALSE(process_running(&child));
    EXPECT_EQ(process_wait(&child), 7);
    EXPECT_EQ(child.pid, -1);

    EXPECT_FALSE(process_spawn(&child, nullptr));
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(process_wait(&child), -1);
}
