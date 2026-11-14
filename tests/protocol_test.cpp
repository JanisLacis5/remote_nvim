#include "protocol/proto.hpp"
#include "test_support.hpp"
#include <gtest/gtest.h>

namespace
{

using namespace test_support;

TEST(ProtocolWrite, VersionedFrameUsesNetworkLengthsAndBinaryFields) {
    socket_pair pair;
    proto::message_handler writer{ networking::socket{ pair.local.release() } };

    ASSERT_TRUE(writer.write_msg({ proto::message_type::save, { std::string("a\0b", 3), "" } }));
    EXPECT_EQ(read_exact_bytes(pair.peer.get(), 17),
              vector_byte({ 'J', 'N', 1, 2, 2, 0, 0, 0, 0, 3, 'a', 0, 'b', 0, 0, 0, 0 }));
}

TEST(ProtocolRead, ReadsIndependentWireFixture) {
    socket_pair pair;
    proto::message_handler reader{ networking::socket{ pair.local.release() } };

    write_all_bytes(pair.peer.get(), vector_byte({ 'J', 'N', 1, 1, 1, 0, 0, 0, 0, 3, '/', 'a', 'b' }));

    auto value = reader.read_msg();

    ASSERT_TRUE(value);
    EXPECT_EQ(value->type, proto::message_type::open);
    EXPECT_EQ(value->fields, std::vector<std::string>{ "/ab" });
}

TEST(ProtocolRead, RejectsInvalidVersionTypeCountReservedAndOversizedLength) {
    for (const auto& bytes :
         { vector_byte({ 'J', 'N', 0, 1, 0, 0 }), vector_byte({ 'J', 'N', 1, 0, 0, 0 }),
           vector_byte({ 'J', 'N', 1, 7, 0, 0 }), vector_byte({ 'J', 'N', 1, 1, 5, 0 }),
           vector_byte({ 'J', 'N', 1, 1, 0, 1 }), vector_byte({ 'J', 'N', 1, 1, 1, 0, 1, 0, 0, 1 }) }) {
        socket_pair pair;
        proto::message_handler reader{ networking::socket{ pair.local.release() } };

        write_all_bytes(pair.peer.get(), bytes);

        EXPECT_FALSE(reader.read_msg());
    }
}

TEST(ProtocolWrite, RejectsOversizedFieldsBeforeWritingAnything) {
    socket_pair pair;
    {
        proto::message_handler writer{ networking::socket{ pair.local.release() } };

        EXPECT_FALSE(writer.write_msg({ proto::message_type::save, { std::string(proto::MAX_FIELD_SIZE + 1, 'x') } }));
        EXPECT_FALSE(writer.write_msg({ proto::message_type::open, { "", "", "", "", "" } }));
    }

    char c;

    EXPECT_EQ(::read(pair.peer.get(), &c, 1), 0);
}

} // namespace
