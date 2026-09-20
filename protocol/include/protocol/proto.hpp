#ifndef PROTO_H
#define PROTO_H

#include <arpa/inet.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <variant>
#include <utility>
#include <string>
#include <array>
#include <span>

namespace proto {

// CONSTANTS
constexpr std::size_t MAX_MESSAGE_LEN  = 4096;
enum class message_type_enum : std::uint8_t {
    open_ui      // sent from server to client to open and attach remote nvim UI window
};

// PAYLOADS (one for each message type)
struct open_ui_payload {
    std::uint16_t port;  // port where neovim server is running (127.0.0.1:<PORT>)
    std::string cwd;     // working dir on the server
};

// CLASS TO GET MESSAGE TYPE FROM PAYLOAD
template <typename T>
struct message_type;

template<>
struct message_type<open_ui_payload> {
    static constexpr auto value = message_type_enum::open_ui;
};

// MESSAGE ITSELF
static constexpr std::size_t HEADER_SIZE = 5;
template <typename Payload>
struct message_header {
    static constexpr auto msg_type = message_type<Payload>::value;
    std::uint32_t payload_size;
};

template <typename Payload>
struct message {
    message_header<Payload> hdr;
    Payload payload;
};

// aliases for message (every message type should have an alias + be added to the std::variant below)
using open_ui_message = message<open_ui_payload>;
using generic_message = std::variant<
    open_ui_message
>;

// encoding / decoding functions
struct encoded_message {
    std::array<std::byte, MAX_MESSAGE_LEN> content;
    std::size_t size{};
};

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

template <typename Payload>
constexpr encoded_message encode(const message<Payload>& message) {
    encoded_message result;
    auto& content = result.content;
    auto& size = result.size;

    auto type = std::to_underlying(message.hdr.msg_type);
    content[size++] = static_cast<std::byte>(type);

    auto payload = encode_payload(message.payload);
    auto payload_size = htonl(static_cast<std::uint32_t>(payload.size));
    if (payload.size + HEADER_SIZE > MAX_MESSAGE_LEN)
        return {};

    std::memcpy(
        content.data() + size,
        &payload_size,
        sizeof(payload_size)
    );
    size += sizeof(payload_size);

    std::copy_n(
        payload.content.begin(),
        payload.size,
        content.begin() + size
    );
    size += payload.size;

    return result;
}

}

#endif

