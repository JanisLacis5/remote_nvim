#ifndef PROTO_H
#define PROTO_H

#include <arpa/inet.h>

#include <algorithm>
#include <optional>
#include <cstring>
#include <variant>
#include <cstdint>
#include <string>
#include <array>
#include <span>

#include "networking/socket.hpp"

namespace proto {

// CONSTANTS
static constexpr std::size_t MAX_MESSAGE_LEN  = 1024;
enum class message_type_enum : std::uint8_t {
    open_ui,      // sent from server to client to open and attach remote nvim UI window
    init_socket   // sent from server to client to tell the type of socket (see networking::sock_type)
};

// PAYLOADS (one for each message type)
struct open_ui_payload {
    std::uint16_t port;  // port where neovim server is running (127.0.0.1:<PORT>)
    std::string cwd;     // working dir on the server
};
struct tell_sck_type_payload {
    networking::sock_type type;   // type of which a socket is being opened
};

// CLASS TO GET MESSAGE TYPE FROM PAYLOAD
template <typename T>
struct message_type;

template<>
struct message_type<open_ui_payload> {
    static constexpr auto value = message_type_enum::open_ui;
};
template<>
struct message_type<tell_sck_type_payload> {
    static constexpr auto value = message_type_enum::init_socket;
};

// MESSAGE ITSELF
static constexpr std::size_t HEADER_SIZE = 5;
template <typename Payload>
struct message_header {
    static constexpr auto msg_type = message_type<Payload>::value;
    std::uint32_t payload_size;
};

struct raw_message_header {
    message_type_enum msg_type;
    std::uint32_t payload_size;
};

template <typename Payload>
struct message {
    message_header<Payload> hdr;
    Payload payload;
};

// aliases for message (every message type should have an alias + be added to the std::variant below)
using tell_sck_type_message = message<tell_sck_type_payload>;
using open_ui_message       = message<open_ui_payload>;

using generic_message = std::variant<
    open_ui_message,
    tell_sck_type_message 
>;

// encoding / decoding functions
struct encoded_message {
    std::array<std::byte, MAX_MESSAGE_LEN> content;
    std::size_t size{};
};

class message_handler
{
public:
    message_handler(networking::socket&& socket);

    networking::socket release_socket();

    generic_message read_msg(); 
    template <typename T>
    bool write_msg(const T& msg) {
        auto message = encode(msg);
        auto written = sck_.write_all(std::span<std::byte>{message.content.data(), message.size});
        return written == message.size;
    }

private:
    networking::socket sck_;

    std::optional<raw_message_header> decode_hdr(std::span<const std::byte> incoming);
    bool decode_open_ui(std::span<const std::byte> incoming, open_ui_message& result);
    bool decode_init_socket(std::span<const std::byte> incoming, tell_sck_type_message& result);
    generic_message decode(const std::span<std::byte> incoming);

    encoded_message encode_payload(const open_ui_payload& payload);
    encoded_message encode_payload(const tell_sck_type_payload& payload);

    template <typename Payload>
    encoded_message encode(const message<Payload>& message) {
        encoded_message result{};
        auto& content = result.content;
        auto& size = result.size;

        auto type = std::to_underlying(message.hdr.msg_type);
        content[size++] = static_cast<std::byte>(type);

        auto payload = encode_payload(message.payload);
        auto payload_size = htonl(static_cast<std::uint32_t>(payload.size));
        if (payload.size + HEADER_SIZE > MAX_MESSAGE_LEN)
            return {};

        std::memcpy(content.data() + size, &payload_size, sizeof(payload_size));
        size += sizeof(payload_size);

        std::copy_n(payload.content.begin(), payload.size, content.begin() + size);
        size += payload.size;

        return result;
    }
};

}

#endif

