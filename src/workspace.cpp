#include "workspace.hpp"
#include "protocol/proto.hpp"
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/xattr.h>
#include <system_error>
#include <unistd.h>

namespace workspace
{

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

descriptor::descriptor(int fd)
    : fd_{ fd } {
    if (fd < 0)
        throw std::system_error(errno, std::generic_category());
}

descriptor::~descriptor() {
    ::close(fd_);
}

namespace
{

bool same(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size &&
           a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
           a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

struct temporary {
    std::string path;

    ~temporary() { ::unlink(path.c_str()); }
};

} // namespace

snapshot read_file(const std::filesystem::path& path) {
    descriptor fd{ ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK) };
    snapshot result;
    require(::fstat(fd.get(), &result.info) == 0 && S_ISREG(result.info.st_mode), "expected a regular file");
    require(result.info.st_size >= 0 && static_cast<std::uint64_t>(result.info.st_size) <= proto::MAX_FIELD_SIZE,
            "file exceeds 16 MiB limit");

    char buffer[65536];
    for (;;) {
        auto count = ::read(fd.get(), buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;

        require(count >= 0, "file read failed");
        if (count == 0)
            break;

        result.content.append(buffer, count);
        require(result.content.size() <= proto::MAX_FIELD_SIZE, "file exceeds 16 MiB limit");
    }

    struct stat after{};
    require(::fstat(fd.get(), &after) == 0 && same(result.info, after), "file changed during read; retry");

    return result;
}

void atomic_write(const std::filesystem::path& path, std::string_view content, unsigned mode) {
    temporary temp{ (path.parent_path() / ".janisnvim-XXXXXX").string() };
    descriptor fd{ ::mkstemp(temp.path.data()) };

    while (!content.empty()) {
        const auto count = ::write(fd.get(), content.data(), content.size());
        if (count < 0 && errno == EINTR)
            continue;

        require(count > 0, "file write failed");
        content.remove_prefix(count);
    }

    require(::fchmod(fd.get(), mode) == 0 && ::fsync(fd.get()) == 0, "cannot persist file");

    require(::rename(temp.path.c_str(), path.c_str()) == 0, "cannot replace file");

    descriptor parent{ ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
    require(::fsync(parent.get()) == 0, "cannot persist directory; save outcome uncertain");
}

void sync_file(const std::filesystem::path& path, snapshot& baseline, std::string_view content) {
    auto current = read_file(path);
    require(current.content == baseline.content && same(current.info, baseline.info),
            "remote conflict: file or metadata changed; local edits retained");

    require(current.info.st_uid == ::geteuid() && current.info.st_nlink == 1 && (current.info.st_mode & 07000) == 0 &&
                ::listxattr(path.c_str(), nullptr, 0) == 0,
            "safe replacement requires an owned, single-link file without special bits or extended attributes");

    // shortcut: optimistic check cannot lock out unrelated writers; use cooperative locking for concurrent writers.
    // Stage in the same directory, preserving ordinary permission bits and group ownership.
    temporary temp{ (path.parent_path() / ".janisnvim-save-XXXXXX").string() };
    descriptor staged{ ::mkstemp(temp.path.data()) };
    require(::fchown(staged.get(), current.info.st_uid, current.info.st_gid) == 0, "cannot preserve ownership");
    require(::flistxattr(staged.get(), nullptr, 0) == 0, "replacement would inherit unsupported extended metadata");

    auto remaining = content;
    while (!remaining.empty()) {
        auto count = ::write(staged.get(), remaining.data(), remaining.size());
        if (count < 0 && errno == EINTR)
            continue;

        require(count > 0, "remote write failed");
        remaining.remove_prefix(count);
    }

    require(::fchmod(staged.get(), current.info.st_mode & 0777) == 0 && ::fsync(staged.get()) == 0,
            "cannot persist remote file");

    auto rechecked = read_file(path);
    require(same(rechecked.info, current.info) && rechecked.content == current.content, "remote conflict during save");

    require(::rename(temp.path.c_str(), path.c_str()) == 0, "remote replacement failed");

    descriptor parent{ ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
    require(::fsync(parent.get()) == 0, "remote directory fsync failed; save outcome uncertain");

    auto updated = read_file(path);
    require(updated.content == content, "remote changed immediately after save");

    baseline = std::move(updated);
}

void private_directory(const std::filesystem::path& path) {
    if (::mkdir(path.c_str(), 0700) != 0 && errno != EEXIST)
        throw std::system_error(errno, std::generic_category(), path.string());

    struct stat info{};
    require(::lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) && info.st_uid == ::geteuid() &&
                (info.st_mode & 077) == 0,
            "cache directory must be owned by you with mode 0700 and not a symlink");
}

bool absolute_path(const std::string& value) {
    if (value.empty() || value.find('\0') != std::string::npos)
        return false;

    std::filesystem::path path{ value };
    return path.is_absolute() && path == path.lexically_normal() && path.filename() != "." && path.filename() != "..";
}

std::filesystem::path cache_entry(const std::filesystem::path& root, const std::string& host,
                                  const std::string& directory, const std::string& path) {
    require(!host.empty() && host.size() <= 128 && absolute_path(directory) && absolute_path(path),
            "invalid remote identity");

    require(!std::filesystem::path(path).lexically_relative(directory).empty() &&
                *std::filesystem::path(path).lexically_relative(directory).begin() != "..",
            "file is outside workspace");

    std::string identity;
    for (const auto& field : { host, directory, path })
        identity += std::to_string(field.size()) + ":" + field;

    constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    for (unsigned char c : identity) {
        encoded += digits[c >> 4];
        encoded += digits[c & 15];
    }

    auto result = root;
    private_directory(result);
    for (std::size_t i = 0; i < encoded.size(); i += 100) {
        result /= encoded.substr(i, 100);
        private_directory(result);
    }

    result /= "entry";
    private_directory(result);

    return result;
}

void prepare_cache(const std::filesystem::path& entry, const std::filesystem::path& file, std::string_view remote) {
    const auto base = entry / "base";
    if (std::filesystem::exists(file)) {
        const auto local = read_file(file).content;
        if (local != remote) {
            require(std::filesystem::exists(base), "cached file has no baseline; manual recovery required");

            const auto previous = read_file(base).content;
            if (remote == previous)
                return; // Resume unsynchronized local edits against the unchanged remote baseline.

            require(local == previous, "remote conflict: cached edits retained; manual reconciliation required");
        }
    }

    atomic_write(base, remote);
    atomic_write(file, remote);
}

} // namespace workspace
