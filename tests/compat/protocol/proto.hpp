#ifndef TEST_PROTO_HPP
#define TEST_PROTO_HPP
#include "networking/socket.hpp"
#include "protocol/proto.h"
#include <optional>
#include <string>
#include <vector>

namespace proto
{
constexpr size_t max_field_size = MAX_FIELD_SIZE;
#undef MAX_FIELD_SIZE
constexpr size_t MAX_FIELD_SIZE = max_field_size;
enum class message_type : uint8_t { open = 1, save, ok, error, close, authenticate };

struct message {
    message_type type;
    std::vector<std::string> fields;
};

class message_handler
{
public:
    explicit message_handler(networking::socket&& socket)
        : socket_(std::move(socket)) {}

    std::optional<message> read_msg() {
        ::message raw{};

        if (!message_read(socket_.native_handle(), &raw))
            return {};

        message result{ static_cast<message_type>(raw.type), {} };

        try {
            for (size_t i = 0; i < raw.count; ++i)
                result.fields.emplace_back(raw.fields[i].data, raw.fields[i].size);
        } catch (...) {
            message_clear(&raw);
            throw;
        }

        message_clear(&raw);
        return result;
    }

    bool write_msg(const message& value) {
        if (value.fields.size() > 4)
            return false;
        ::message raw{};

        raw.type = static_cast<::message_type>(value.type);
        raw.count = value.fields.size();
        for (size_t i = 0; i < raw.count; ++i)
            raw.fields[i] = { const_cast<char*>(value.fields[i].data()), value.fields[i].size() };

        return message_write(socket_.native_handle(), &raw);
    }

private:
    networking::socket socket_;
};

} // namespace proto
#endif
