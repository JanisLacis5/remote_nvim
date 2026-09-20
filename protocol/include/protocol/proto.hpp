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
static constexpr std::size_t HEADER_SIZE = 1;
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

constexpr generic_message decode(std::span<const std::byte> incoming);
constexpr open_ui_message decode_open_ui(std::span<const std::byte> incoming);
constexpr encoded_message encode_payload(const open_ui_payload& payload);

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

