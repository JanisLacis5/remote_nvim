#ifndef PROTO_H
#define PROTO_H

#include <cstdint>
#include <variant>
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
template <typename Payload>
struct message_header {
    static constexpr auto msg_type = message_type<Payload>::value;
    std::size_t payload_len;
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

generic_message decode(const std::span<std::byte> incoming);

template <typename Payload>
encoded_message encode(const message<Payload>& message) {
    return {};
}

}

#endif

