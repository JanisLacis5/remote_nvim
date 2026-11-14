#ifndef TEST_WORKSPACE_HPP
#define TEST_WORKSPACE_HPP
#include "workspace.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
namespace workspace {
inline void require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
class descriptor {
public:
    explicit descriptor(int fd) : fd_(fd) { if (fd < 0) throw std::system_error(errno, std::generic_category()); }
    ~descriptor() { ::close(fd_); }
    descriptor(const descriptor&) = delete;
    descriptor& operator=(const descriptor&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};
struct snapshot { std::string content; struct stat info{}; };
inline field bytes(std::string_view value) { return { const_cast<char*>(value.data()), value.size() }; }
inline snapshot read_file(const std::filesystem::path& path) {
    ::snapshot raw{};
    const char* error = nullptr;
    require(ws_read(path.c_str(), &raw, &error), error ? error : "read failed");
    snapshot result;
    try { result = { std::string(raw.content.data, raw.content.size), raw.info }; }
    catch (...) { ws_snapshot_clear(&raw); throw; }
    ws_snapshot_clear(&raw);
    return result;
}
inline void atomic_write(const std::filesystem::path& path, std::string_view content, unsigned mode = 0600) {
    const char* error = nullptr;
    bool ok = ws_atomic_write(path.c_str(), bytes(content), mode, &error);
    require(ok, error);
}
inline void sync_file(const std::filesystem::path& path, snapshot& baseline, std::string_view content) {
    ::snapshot raw{};
    raw.content = { static_cast<char*>(malloc(baseline.content.size() + 1)), baseline.content.size() };
    if (!raw.content.data) throw std::bad_alloc();
    std::copy(baseline.content.begin(), baseline.content.end(), raw.content.data);
    raw.info = baseline.info;
    const char* error = nullptr;
    bool ok = ws_sync(path.c_str(), &raw, bytes(content), &error);
    if (ok) baseline = { std::string(raw.content.data, raw.content.size), raw.info };
    ws_snapshot_clear(&raw);
    require(ok, error);
}
inline void private_directory(const std::filesystem::path& path) {
    const char* error = nullptr;
    bool ok = ws_private_directory(path.c_str(), &error);
    require(ok, error);
}
inline bool absolute_path(const std::string& path) {
    return path.find('\0') == std::string::npos && ws_absolute_path(path.c_str());
}
inline std::filesystem::path cache_entry(const std::filesystem::path& root, const std::string& host,
                                        const std::string& directory, const std::string& path) {
    require(absolute_path(directory) && absolute_path(path), "invalid identity");
    const char* error = nullptr;
    char* entry = ws_cache_entry(root.c_str(), host.c_str(), directory.c_str(), path.c_str(), &error);
    require(entry, error);
    std::filesystem::path result(entry);
    free(entry);
    return result;
}
inline void prepare_cache(const std::filesystem::path& entry, const std::filesystem::path& file, std::string_view remote) {
    const char* error = nullptr;
    bool ok = ws_prepare_cache(entry.c_str(), file.c_str(), bytes(remote), &error);
    require(ok, error);
}
}
#endif
