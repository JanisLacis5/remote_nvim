#include "protocol/proto.hpp"

namespace proto {

constexpr open_ui_message decode_open_ui(std::span<const std::byte> incoming) {
    if (incoming.size() < HEADER_SIZE)
        return {};

    open_ui_message result;
    auto* ptr = incoming.data() + 1;
    
    std::uint32_t payload_size;
    std::memcpy(&payload_size, ptr, sizeof(payload_size));
    ptr += sizeof(payload_size);
    payload_size = htonl(payload_size);
    if (payload_size > MAX_MESSAGE_LEN - HEADER_SIZE)
        return {};
    result.hdr.payload_size = payload_size;

    std::uint16_t port;
    std::memcpy(&port, ptr, sizeof(port));
    ptr += sizeof(port);
    result.payload.port= ntohs(port);

    const std::size_t cwd_size = result.hdr.payload_size - sizeof(port);
    result.payload.cwd = std::string(
        reinterpret_cast<const char*>(ptr),
        cwd_size
    );

    return result;
}

constexpr generic_message decode(const std::span<std::byte> incoming) {
    auto type = static_cast<message_type_enum>(incoming[0]);

    switch (type) {
    case proto::message_type_enum::open_ui:
        return decode_open_ui(incoming);
    }

    return {};
}

constexpr encoded_message encode_payload(const open_ui_payload& payload) {
    if (payload.cwd.size() + sizeof(uint16_t) > MAX_MESSAGE_LEN)
        return {};

    encoded_message result;

    std::uint16_t port = htons(payload.port);
    std::memcpy(result.content.begin(), &port, sizeof(port));
    result.size += sizeof(port);

    std::memcpy(result.content.data() + result.size, payload.cwd.data(), payload.cwd.size());
    result.size += payload.cwd.size();

    return result;
}

}
