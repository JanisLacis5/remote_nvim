#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <filesystem>
#include <string>
#include <string_view>
#include <sys/stat.h>

namespace workspace
{

class descriptor
{
public:
    explicit descriptor(int fd);
    ~descriptor();

    descriptor(const descriptor&) = delete;
    descriptor& operator=(const descriptor&) = delete;

    int get() const { return fd_; }

private:
    int fd_;
};

struct snapshot {
    std::string content;
    struct stat info{};
};

snapshot read_file(const std::filesystem::path& path);
void atomic_write(const std::filesystem::path& path, std::string_view content, unsigned mode = 0600);
void sync_file(const std::filesystem::path& path, snapshot& baseline, std::string_view content);

void private_directory(const std::filesystem::path& path);
std::filesystem::path cache_entry(const std::filesystem::path& root, const std::string& host,
                                  const std::string& directory, const std::string& path);
void prepare_cache(const std::filesystem::path& entry, const std::filesystem::path& file, std::string_view remote);

bool absolute_path(const std::string& path);
void require(bool condition, const char* message);

} // namespace workspace

#endif
