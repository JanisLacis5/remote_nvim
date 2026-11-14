// Local service and asynchronous save helper.
#include "networking/listener.hpp"
#include "os/process.hpp"
#include "protocol/proto.hpp"
#include "workspace.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace
{

using workspace::require;

void timeout(int fd) {
    const timeval limit{ 30, 0 };
    require(::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit)) == 0 &&
                ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof(limit)) == 0,
            "cannot set transport timeout");
}

sockaddr_un address(const std::string& path) {
    sockaddr_un result{};
    result.sun_family = AF_UNIX;
    require(path.size() < sizeof(result.sun_path), "local socket path too long");

    std::memcpy(result.sun_path, path.c_str(), path.size() + 1);

    return result;
}

int save(const char* socket_path, const char* file) {
    auto content = workspace::read_file(file).content;

    networking::socket connection{ AF_UNIX, SOCK_STREAM };
    timeout(connection.native_handle());

    auto addr = address(socket_path);
    require(::connect(connection.native_handle(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0,
            "session unavailable; saved local copy retained; reopen remotely to reconnect");

    proto::message_handler channel{ std::move(connection) };
    require(channel.write_msg({ proto::message_type::save, { std::move(content) } }),
            "cannot queue save; local copy retained");

    auto response = channel.read_msg();
    require(response.has_value(), "sync timed out or disconnected; remote outcome uncertain, local copy retained");
    if (response->type == proto::message_type::error && response->fields.size() == 1)
        throw std::runtime_error(response->fields[0]);
    require(response->type == proto::message_type::ok && response->fields.empty(), "invalid sync response");

    std::cout << "Remote file synchronized\n";

    return 0;
}

struct runtime_directory {
    std::string path{ "/tmp/janisnvim-XXXXXX" };

    runtime_directory() { require(::mkdtemp(path.data()) != nullptr, "cannot create session directory"); }

    ~runtime_directory() {
        ::unlink((path + "/sync.sock").c_str());
        ::rmdir(path.c_str());
    }
};

void session(proto::message_handler& remote, const proto::message& request, const std::filesystem::path& root) {
    require(request.type == proto::message_type::open && request.fields.size() == 4, "expected an open-file request");

    auto entry = workspace::cache_entry(root, request.fields[0], request.fields[1], request.fields[2]);
    const auto working = entry / "working";
    workspace::private_directory(working);

    const auto file = working / std::filesystem::path(request.fields[2]).filename();
    workspace::descriptor lock{ ::open((entry / "lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600) };
    require(::flock(lock.get(), LOCK_EX | LOCK_NB) == 0, "cached file already open");

    try {
        workspace::prepare_cache(entry, file, request.fields[3]);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string(error.what()) + "; cache: " + file.string());
    }

    runtime_directory runtime;
    const auto socket_path = runtime.path + "/sync.sock";
    networking::socket listener{ AF_UNIX, SOCK_STREAM };

    auto addr = address(socket_path);
    require(::bind(listener.native_handle(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 &&
                ::listen(listener.native_handle(), 16) == 0,
            "cannot listen for local saves");

    const auto executable = std::filesystem::read_symlink("/proc/self/exe").string();
    require(::setenv("JANISNVIM_HELPER", executable.c_str(), 1) == 0 &&
                ::setenv("JANISNVIM_SESSION", socket_path.c_str(), 1) == 0 &&
                ::setenv("JANISNVIM_FILE", file.c_str(), 1) == 0 && ::setenv("JANISNVIM_LUA", JANISNVIM_LUA, 1) == 0,
            "cannot configure editor environment");

    const char* terminal = std::getenv("JANISNVIM_TERMINAL");
    if (!terminal || !*terminal)
        terminal = "kitty";

    process editor{ terminal, "nvim", "-c", "lua dofile(vim.env.JANISNVIM_LUA)", "--", file.string() };
    require(remote.write_msg({ proto::message_type::ok, { file.string() } }), "open acknowledgement failed");
    std::cerr << "Editing " << request.fields[2] << " in " << file << '\n';

    bool connected = true;
    while (editor.running()) {
        pollfd waiting{ listener.native_handle(), POLLIN, 0 };
        const auto ready = ::poll(&waiting, 1, 200);
        if (ready < 0 && errno == EINTR)
            continue;
        require(ready >= 0, "local save polling failed");
        if (!(waiting.revents & POLLIN))
            continue;

        networking::socket peer{ ::accept4(listener.native_handle(), nullptr, nullptr, SOCK_CLOEXEC) };
        if (!peer.is_valid())
            continue;

        timeout(peer.native_handle());
        proto::message_handler local{ std::move(peer) };

        try {
            auto update = local.read_msg();
            require(update && update->type == proto::message_type::save && update->fields.size() == 1,
                    "invalid local save");
            require(connected, "remote connection lost; reopen remotely to reconnect; local copy retained");

            if (!remote.write_msg(*update)) {
                connected = false;
                throw std::runtime_error("remote connection lost; local copy retained");
            }

            auto reply = remote.read_msg();
            if (!reply) {
                connected = false;
                throw std::runtime_error("remote save outcome uncertain; local copy retained");
            }

            if (reply->type == proto::message_type::error && reply->fields.size() == 1)
                throw std::runtime_error(reply->fields[0]);
            require(reply->type == proto::message_type::ok && reply->fields.empty(), "invalid remote acknowledgement");

            workspace::atomic_write(entry / "base", update->fields[0]);
            local.write_msg({ proto::message_type::ok, {} });
        } catch (const std::exception& error) {
            std::cerr << "Sync: " << error.what() << '\n';
            local.write_msg({ proto::message_type::error, { error.what() } });
        }
    }

    require(editor.wait() == 0, "terminal/editor exited unsuccessfully; cached file retained");
    if (connected)
        remote.write_msg({ proto::message_type::close, {} });
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--sync")
            return save(argv[2], argv[3]);

        require(argc == 1, "usage: janisnvimdaemon");

        const char* token = std::getenv("JANISNVIM_TOKEN");
        require(token && std::string_view(token).size() >= 32,
                "set JANISNVIM_TOKEN to a shared random secret (at least 32 characters)");
        const std::string secret{ token };

        const char* cache = std::getenv("XDG_CACHE_HOME");
        const char* home = std::getenv("HOME");
        require((cache && *cache) || (home && *home), "HOME or XDG_CACHE_HOME is required");
        auto parent = cache && *cache ? std::filesystem::path(cache) : std::filesystem::path(home) / ".cache";
        require(parent.is_absolute(), "cache root must be absolute");

        std::filesystem::create_directories(parent);
        const auto root = parent / "janisnvim";
        workspace::private_directory(root);

        networking::listener listener{};
        require(listener.is_valid(), "cannot listen on 127.0.0.1:7777");

        // shortcut: one editor session at a time; add per-session workers when simultaneous sessions are needed.
        for (;;) {
            auto accepted = listener.accept();
            if (!accepted)
                continue;

            timeout(accepted->native_handle());
            proto::message_handler remote{ std::move(*accepted) };

            try {
                const auto auth = remote.read_msg();
                require(auth && auth->type == proto::message_type::authenticate && auth->fields.size() == 1 &&
                            auth->fields[0] == secret,
                        "authentication failed");
                require(remote.write_msg({ proto::message_type::ok, {} }), "authentication acknowledgement failed");

                auto request = remote.read_msg();
                require(request.has_value(), "file request missing or malformed");
                session(remote, *request, root);
            } catch (const std::exception& error) {
                std::cerr << "janisnvimdaemon: " << error.what() << '\n';
                remote.write_msg({ proto::message_type::error, { error.what() } });
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "janisnvimdaemon: " << error.what() << '\n';
        return 1;
    }
}
