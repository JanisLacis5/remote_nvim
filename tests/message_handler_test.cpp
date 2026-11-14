#include "protocol/proto.hpp"
#include "test_support.hpp"
#include <future>
#include <gtest/gtest.h>

namespace
{

using namespace test_support;

TEST(FileMessages, FragmentedFramesRemainSeparate) {
    socket_pair pair;
    proto::message_handler reader{ networking::socket{ pair.local.release() } };
    const auto bytes = vector_byte({ 'J', 'N', 1, 2, 1, 0, 0, 0, 0, 3, 'a', 'b', 'c', 'J', 'N', 1, 3, 0, 0 });

    auto writer = std::async(std::launch::async, [&] {
        for (const auto byte : bytes)
            write_all_bytes(pair.peer.get(), std::span{ &byte, 1 });
    });

    auto first = reader.read_msg();
    auto second = reader.read_msg();

    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(first->fields, std::vector<std::string>{ "abc" });
    EXPECT_EQ(second->type, proto::message_type::ok);
    EXPECT_TRUE(second->fields.empty());

    writer.get();
}

TEST(FileMessages, TruncatedHeaderLengthOrContentIsNotAMessage) {
    const auto bytes = vector_byte({ 'J', 'N', 1, 2, 1, 0, 0, 0, 0, 3, 'a', 'b', 'c' });
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        socket_pair pair;
        proto::message_handler reader{ networking::socket{ pair.local.release() } };

        write_all_bytes(pair.peer.get(), std::span{ bytes }.first(n));
        ::shutdown(pair.peer.get(), SHUT_WR);

        EXPECT_FALSE(reader.read_msg()) << n;
    }
}

TEST(FileMessages, LargeFileAndEmptyFileRoundTrip) {
    for (const auto size : { 0u, 65537u, 1024u * 1024u }) {
        socket_pair pair;
        proto::message_handler writer{ networking::socket{ pair.local.release() } };
        proto::message_handler reader{ networking::socket{ pair.peer.release() } };

        std::string content(size, 'x');
        auto writing = std::async(std::launch::async,
                                  [&] { return writer.write_msg({ proto::message_type::save, { content } }); });

        auto result = reader.read_msg();

        ASSERT_TRUE(result);
        EXPECT_EQ(result->fields, std::vector<std::string>{ content });
        EXPECT_TRUE(writing.get());
    }
}

TEST(FileMessages, DisconnectedWriteDoesNotRaiseSigpipe) {
    socket_pair pair;
    proto::message_handler writer{ networking::socket{ pair.local.release() } };

    pair.peer.reset();

    EXPECT_FALSE(writer.write_msg({ proto::message_type::ok, {} }));
}

} // namespace
