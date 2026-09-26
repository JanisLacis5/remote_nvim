#include "protocol/proto.hpp"

#include <cstdlib>

namespace proto {

message_handler::message_handler(networking::socket&& socket)
    : sck_{std::move(socket)}
{}

generic_message message_handler::read_msg() {
    auto raw = sck_.read(HEADER_SIZE);
    if (raw.empty()) 
        return {};

    auto maybe_raw_hdr = decode_hdr(raw);
    if (!maybe_raw_hdr.has_value())
        return {};
    auto& raw_hdr = maybe_raw_hdr.value();

    auto raw_payload = sck_.read(raw_hdr.payload_size);
    if (raw_payload.empty())
        return {};

    std::array<std::byte, MAX_MESSAGE_LEN> msg;
    std::copy_n(raw.begin(), HEADER_SIZE, msg.begin());
    std::copy_n(raw_payload.begin(), raw_payload.size(), msg.begin() + HEADER_SIZE);

    return decode(msg);
}

networking::socket message_handler::release_socket() {
    return std::move(sck_);
}

std::optional<raw_message_header> message_handler::decode_hdr(std::span<const std::byte> incoming) {
    if (incoming.size() < HEADER_SIZE)
        return {};

    raw_message_header hdr;
    auto* ptr = incoming.data();

    auto integral = static_cast<std::underlying_type_t<message_type_enum>>(*ptr);
    hdr.msg_type = static_cast<message_type_enum>(integral);
    ptr++;

    std::uint32_t payload_size;
    std::memcpy(&payload_size, ptr, sizeof(payload_size));
    hdr.payload_size = ntohl(payload_size);

    return hdr;
}

bool message_handler::decode_init_socket(std::span<const std::byte> incoming, tell_sck_type_message& result) {
    // assert because endianness is ignored in this function
    static_assert(sizeof(result.payload.type) == 1);

    if (incoming.size() < HEADER_SIZE)
        return false;

    auto* ptr = incoming.data() + HEADER_SIZE; 
    std::memcpy(&result.payload.type, ptr, sizeof(result.payload.type));

    return true;
}

bool message_handler::decode_open_ui(std::span<const std::byte> incoming, open_ui_message& result) {
    if (incoming.size() < HEADER_SIZE)
        return false;
    
    auto* ptr = incoming.data() + HEADER_SIZE;

    std::uint16_t port;
    std::memcpy(&port, ptr, sizeof(port));
    ptr += sizeof(port);
    result.payload.port= ntohs(port);

    const std::size_t cwd_size = result.hdr.payload_size - sizeof(port);
    result.payload.cwd = std::string(
        reinterpret_cast<const char*>(ptr),
        cwd_size
    );

    return true;
}

generic_message message_handler::decode(const std::span<std::byte> incoming) {
    if (incoming.size() < HEADER_SIZE)
        return {};

    auto raw_hdr = decode_hdr(incoming).value();

    switch (raw_hdr.msg_type) {
    case proto::message_type_enum::open_ui:
    {
        open_ui_message result;
        result.hdr.payload_size = raw_hdr.payload_size;
        decode_open_ui(incoming, result);
        return result;
    }
    case proto::message_type_enum::init_socket:
    {
        tell_sck_type_message result;
        result.hdr.payload_size = raw_hdr.payload_size;
        decode_init_socket(incoming, result);
        return result;
    }
    }

    return {};
}

encoded_message message_handler::encode_payload(const open_ui_payload& payload) {
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

encoded_message message_handler::encode_payload(const tell_sck_type_payload& payload) {
    // endinanness ignored
    static_assert(sizeof(payload.type) == 1);

    encoded_message result;

    std::memcpy(result.content.data(), &payload.type, sizeof(payload.type));
    result.size = sizeof(payload.type);

    return result;
}

}
