#include "protocol/proto.hpp"

namespace proto {

constexpr generic_message decode(const std::span<std::byte> incoming) {
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
