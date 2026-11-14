// Invoked on the remote host; owns one file session, never an editor.
#include "networking/socket.hpp"
#include "protocol/proto.hpp"
#include "workspace.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/file.h>
#include <unistd.h>

int main(int argc, char** argv) {
    using workspace::require;

    try {
        require(argc == 2, "usage: janisnvim path/to/existing-file");

        const char* token = std::getenv("JANISNVIM_TOKEN");
        require(token && std::string_view(token).size() >= 32,
                "set JANISNVIM_TOKEN to the daemon's shared secret (at least 32 characters)");

        const auto directory = std::filesystem::canonical(std::filesystem::current_path());
        const auto path = std::filesystem::canonical(argv[1]);
        const auto relative = path.lexically_relative(directory);
        require(!relative.empty() && *relative.begin() != "..",
                "file must resolve inside the current workspace directory");

        auto baseline = workspace::read_file(path);

        auto host = workspace::read_file("/etc/machine-id").content;
        while (!host.empty() && (host.back() == '\n' || host.back() == '\r'))
            host.pop_back();

        require(!host.empty(), "machine-id is empty");
        host += ":" + std::to_string(::geteuid());

        // Cooperating sessions lock a stable sidecar across atomic replacements.
        const auto lock_path = path.parent_path() / (".janisnvim-" + path.filename().string() + ".lock");
        workspace::descriptor lock{ ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600) };
        require(::flock(lock.get(), LOCK_EX | LOCK_NB) == 0, "file already open in another janisnvim session");

        networking::socket socket{ AF_INET, SOCK_STREAM };
        require(socket.connect(INADDR_LOOPBACK, 7778),
                "cannot reach local daemon via SSH reverse forward on port 7778");

        proto::message_handler channel{ std::move(socket) };

        require(channel.write_msg({ proto::message_type::authenticate, { token } }), "authentication send failed");

        auto authenticated = channel.read_msg();
        require(authenticated && authenticated->type == proto::message_type::ok, "daemon authentication failed");

        require(channel.write_msg(
                    { proto::message_type::open, { host, directory.string(), path.string(), baseline.content } }),
                "file transfer failed");

        auto opened = channel.read_msg();
        if (opened && opened->type == proto::message_type::error && opened->fields.size() == 1)
            throw std::runtime_error(opened->fields[0]);
        require(opened && opened->type == proto::message_type::ok && opened->fields.size() == 1,
                "local editor failed to open");

        std::cout << "Local working copy: " << opened->fields[0] << '\n';

        while (auto request = channel.read_msg()) {
            if (request->type == proto::message_type::error && request->fields.size() == 1)
                throw std::runtime_error(request->fields[0]);

            if (request->type == proto::message_type::close && request->fields.empty())
                return 0;

            require(request->type == proto::message_type::save && request->fields.size() == 1, "invalid save request");

            try {
                workspace::sync_file(path, baseline, request->fields[0]);

                require(channel.write_msg({ proto::message_type::ok, {} }), "save acknowledgement failed");
            } catch (const std::exception& error) {
                require(channel.write_msg({ proto::message_type::error, { error.what() } }),
                        "connection lost during save");
            }
        }

        throw std::runtime_error("connection lost; local cached edits are retained");
    } catch (const std::exception& error) {
        std::cerr << "janisnvim: " << error.what() << '\n';
        return 1;
    }
}
