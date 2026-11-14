#include <cerrno>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace
{
bool write_all(int fd, const std::string& text) {
    std::size_t offset{};

    while (offset < text.size()) {
        const auto count = ::write(fd, text.data() + offset, text.size() - offset);
        if (count < 0 && errno == EINTR)
            continue;

        if (count <= 0)
            return false;

        offset += static_cast<std::size_t>(count);
    }

    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3)
        return 1;

    const std::string mode{ argv[1] };
    if (mode == "exit")
        return std::atoi(argv[2]);

    const auto fd = std::atoi(argv[2]);

    if (mode == "write") {
        for (int i{ 3 }; i < argc; ++i) {
            std::string argument_line{ argv[i] };
            argument_line += '\n';
            const auto written = write_all(fd, argument_line);
            if (!written)
                return 2;
        }

        return 0;
    }

    if (mode == "wait" && argc == 4) {
        if (::chdir(argv[3]) < 0 || !write_all(fd, "R"))
            return 3;

        char byte{ '\0' };

        while (::read(fd, &byte, 1) < 0 && errno == EINTR) {
        }

        return 0;
    }

    return 4;
}
