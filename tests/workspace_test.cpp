#include "workspace.hpp"
#include <gtest/gtest.h>
#include <sys/xattr.h>
#include <unistd.h>

namespace
{

class Workspace : public testing::Test
{
protected:
    std::filesystem::path root;

    void SetUp() override {
        char path[] = "/tmp/janisnvim-test-XXXXXX";

        ASSERT_NE(::mkdtemp(path), nullptr);
        root = path;
    }

    void TearDown() override { std::filesystem::remove_all(root); }
};

TEST_F(Workspace, IdentitySeparatesHostsWorkspacesAndPaths) {
    const auto a = workspace::cache_entry(root, "host1", "/project", "/project/a.cpp");

    EXPECT_EQ(a, workspace::cache_entry(root, "host1", "/project", "/project/a.cpp"));
    EXPECT_NE(a, workspace::cache_entry(root, "host2", "/project", "/project/a.cpp"));
    EXPECT_NE(a, workspace::cache_entry(root, "host1", "/", "/project/a.cpp"));
    EXPECT_NE(a, workspace::cache_entry(root, "host1", "/project", "/project/b.cpp"));
    EXPECT_THROW(workspace::cache_entry(root, "host", "/project", "/project/../outside"), std::runtime_error);
    EXPECT_THROW(workspace::cache_entry(root, "host", "/project", "/outside"), std::runtime_error);
    EXPECT_THROW(workspace::cache_entry(root, "host", "/project", std::string("/project/a\0b", 12)),
                 std::runtime_error);
}

TEST_F(Workspace, CacheDirectoryRejectsSymlinksAndPublicPermissions) {
    std::filesystem::create_directory_symlink(root, root / "link");

    EXPECT_THROW(workspace::private_directory(root / "link"), std::runtime_error);

    std::filesystem::create_directory(root / "public");
    ::chmod((root / "public").c_str(), 0755);

    EXPECT_THROW(workspace::private_directory(root / "public"), std::runtime_error);
}

TEST_F(Workspace, SavedBytesReplaceRemoteAndPreservePermissions) {
    const auto path = root / "a file;$.cpp";
    workspace::atomic_write(path, "old", 0750);
    auto baseline = workspace::read_file(path);

    const std::string binary("new\0data", 8);
    workspace::sync_file(path, baseline, binary);

    EXPECT_EQ(workspace::read_file(path).content, binary);
    EXPECT_EQ(workspace::read_file(path).info.st_mode & 0777, 0750u);

    workspace::sync_file(path, baseline, "");

    EXPECT_TRUE(workspace::read_file(path).content.empty());
}

TEST_F(Workspace, ConflictNeverOverwritesRemote) {
    const auto path = root / "file";
    workspace::atomic_write(path, "old");
    auto baseline = workspace::read_file(path);

    workspace::atomic_write(path, "remote change");

    EXPECT_THROW(workspace::sync_file(path, baseline, "local change"), std::runtime_error);
    EXPECT_EQ(workspace::read_file(path).content, "remote change");
    EXPECT_EQ(baseline.content, "old");
}

TEST_F(Workspace, MetadataChangesAndHardlinksAreRejected) {
    const auto path = root / "file";
    workspace::atomic_write(path, "old");
    auto baseline = workspace::read_file(path);

    ::chmod(path.c_str(), 0644);

    EXPECT_THROW(workspace::sync_file(path, baseline, "new"), std::runtime_error);

    std::filesystem::create_hard_link(path, root / "hardlink");
    baseline = workspace::read_file(path);

    EXPECT_THROW(workspace::sync_file(path, baseline, "new"), std::runtime_error);
    EXPECT_EQ(workspace::read_file(path).content, "old");
}

TEST_F(Workspace, ExtendedMetadataIsRejectedWithoutChangingContents) {
    const auto path = root / "file";
    workspace::atomic_write(path, "old");

    if (::setxattr(path.c_str(), "user.janisnvim-test", "value", 5, 0) != 0)
        GTEST_SKIP() << "filesystem does not support user extended attributes";

    auto baseline = workspace::read_file(path);

    EXPECT_THROW(workspace::sync_file(path, baseline, "new"), std::runtime_error);
    EXPECT_EQ(workspace::read_file(path).content, "old");
}

TEST_F(Workspace, SymlinkReplacementCannotRedirectWrite) {
    const auto path = root / "file";
    const auto target = root / "target";
    workspace::atomic_write(path, "old");
    workspace::atomic_write(target, "unrelated");
    auto baseline = workspace::read_file(path);

    std::filesystem::remove(path);
    std::filesystem::create_symlink(target, path);

    EXPECT_THROW(workspace::sync_file(path, baseline, "new"), std::exception);
    EXPECT_EQ(workspace::read_file(target).content, "unrelated");
}

TEST_F(Workspace, CacheResumesDirtyEditsAndRejectsConcurrentRemoteChanges) {
    const auto file = root / "working";
    workspace::prepare_cache(root, file, "old");

    workspace::atomic_write(file, "local change");
    workspace::prepare_cache(root, file, "old");

    EXPECT_EQ(workspace::read_file(file).content, "local change");
    EXPECT_THROW(workspace::prepare_cache(root, file, "remote change"), std::runtime_error);
    EXPECT_EQ(workspace::read_file(file).content, "local change");
    EXPECT_EQ(workspace::read_file(root / "base").content, "old");

    // A lost acknowledgement is recovered when remote and cached contents agree.
    workspace::prepare_cache(root, file, "local change");

    EXPECT_EQ(workspace::read_file(root / "base").content, "local change");
}

TEST_F(Workspace, CleanCacheRefreshesAndMissingBaselinePreservesEdits) {
    const auto file = root / "working";
    workspace::prepare_cache(root, file, "old");

    workspace::prepare_cache(root, file, "new remote");

    EXPECT_EQ(workspace::read_file(file).content, "new remote");

    std::filesystem::remove(root / "base");

    EXPECT_THROW(workspace::prepare_cache(root, file, "other"), std::runtime_error);
    EXPECT_EQ(workspace::read_file(file).content, "new remote");
}

} // namespace
