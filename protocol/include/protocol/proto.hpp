#ifndef PROTO_H
#define PROTO_H

#include "networking/socket.hpp"
#include <optional>
#include <string>
#include <vector>

namespace proto
{

// A bounded, versioned frame; fields are binary-safe length-prefixed strings.
constexpr std::size_t MAX_FIELD_SIZE = 16 * 1024 * 1024;

enum class message_type : std::uint8_t { open = 1, save, ok, error, close, authenticate };

struct message {
    message_type type;
    std::vector<std::string> fields;
};

class message_handler
{
public:
    explicit message_handler(networking::socket&& socket);

    std::optional<message> read_msg();
    bool write_msg(const message& message);

private:
    networking::socket socket_;
};

} // namespace proto

#endif
