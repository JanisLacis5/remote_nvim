#include "protocol/proto.hpp"
#include <cstring>

namespace proto
{

message_handler::message_handler(networking::socket&& socket)
    : socket_{ std::move(socket) } {}

std::optional<message> message_handler::read_msg() {
    const auto header = socket_.read(6);
    if (header.size() != 6 || header[0] != std::byte{ 'J' } || header[1] != std::byte{ 'N' } ||
        header[2] != std::byte{ 1 } || header[3] < std::byte{ 1 } || header[3] > std::byte{ 6 } ||
        header[4] > std::byte{ 4 } || header[5] != std::byte{ 0 })
        return {};

    message result{ static_cast<message_type>(header[3]), {} };
    for (unsigned i = 0; i < std::to_integer<unsigned>(header[4]); ++i) {
        auto length = socket_.read(4);
        if (length.size() != 4)
            return {};

        std::uint32_t size;
        std::memcpy(&size, length.data(), 4);
        size = ntohl(size);
        if (size > MAX_FIELD_SIZE)
            return {};

        auto bytes = socket_.read(size);
        if (bytes.size() != size)
            return {};

        result.fields.emplace_back(bytes.empty() ? "" : reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    return result;
}

bool message_handler::write_msg(const message& message) {
    if (message.fields.size() > 4 || message.type < message_type::open || message.type > message_type::authenticate)
        return false;

    for (const auto& field : message.fields)
        if (field.size() > MAX_FIELD_SIZE)
            return false;

    const std::byte header[]{ std::byte{ 'J' },
                              std::byte{ 'N' },
                              std::byte{ 1 },
                              static_cast<std::byte>(message.type),
                              static_cast<std::byte>(message.fields.size()),
                              std::byte{ 0 } };
    if (socket_.write_all(header) != sizeof(header))
        return false;

    for (const auto& field : message.fields) {
        const auto size = htonl(static_cast<std::uint32_t>(field.size()));
        const auto length = std::as_bytes(std::span{ &size, 1 });
        const auto bytes = std::as_bytes(std::span{ field.data(), field.size() });

        if (socket_.write_all(length) != length.size() || socket_.write_all(bytes) != bytes.size())
            return false;
    }

    return true;
}

} // namespace proto
