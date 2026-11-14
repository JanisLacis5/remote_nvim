#include "networking/listener.hpp"
#include "test_support.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
using namespace test_support;

TEST(Listener, IsMoveOnlyAndMovesWithoutThrowing) {
    EXPECT_FALSE(std::is_copy_constructible_v<networking::listener>);
    EXPECT_FALSE(std::is_copy_assignable_v<networking::listener>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<networking::listener>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<networking::listener>);
}

TEST(Listener, EphemeralPortsAreDistinct) {
    networking::listener first{ 0 };
    networking::listener second{ 0 };

    ASSERT_TRUE(first.is_valid());
    ASSERT_TRUE(second.is_valid());
    EXPECT_NE(first.port(), 0);
    EXPECT_NE(first.port(), second.port());
}

TEST(Listener, AcceptsRawTcpPeerAndExchangesBytesInBothDirections) {
    networking::listener listener{ 0 };

    ASSERT_TRUE(listener.is_valid());

    auto peer = connect_tcp_peer(listener.port());
    auto accepted = listener.accept();

    ASSERT_TRUE(accepted.has_value());
    ASSERT_TRUE(accepted->is_valid());

    const auto outgoing_bytes = vector_byte({ 0, 1, 255 });

    write_all_bytes(peer.get(), outgoing_bytes);

    const auto expected_bytes = vector_byte({ 0, 1, 255 });
    const auto received_bytes = accepted->read(3);

    EXPECT_EQ(received_bytes, expected_bytes);
    const auto outgoing_bytes_second = vector_byte({ 42, 128 });
    const auto bytes_written = accepted->write_all(outgoing_bytes_second);

    ASSERT_EQ(bytes_written, 2u);
    const auto expected_bytes_second = vector_byte({ 42, 128 });
    const auto received_bytes_second = read_exact_bytes(peer.get(), 2);

    EXPECT_EQ(received_bytes_second, expected_bytes_second);
}

TEST(Listener, WrapperConnectsUsingHostOrderAddressAndPort) {
    networking::listener listener{ 0 };

    ASSERT_TRUE(listener.is_valid());

    networking::socket client{ AF_INET, SOCK_STREAM };

    const auto connected = client.connect(INADDR_LOOPBACK, listener.port());

    ASSERT_TRUE(connected);

    auto server = listener.accept();

    ASSERT_TRUE(server.has_value());
    const auto outgoing_bytes = vector_byte({ 1, 2 });
    const auto bytes_written = client.write_all(outgoing_bytes);

    ASSERT_EQ(bytes_written, 2u);
    const auto expected_bytes = vector_byte({ 1, 2 });
    const auto received_bytes = server->read(2);

    EXPECT_EQ(received_bytes, expected_bytes);
}

TEST(Listener, DuplicatePortIsInvalidAndAcceptReturnsEmpty) {
    networking::listener original{ 0 };

    ASSERT_TRUE(original.is_valid());

    networking::listener duplicate{ original.port() };

    EXPECT_FALSE(duplicate.is_valid());
    EXPECT_EQ(duplicate.port(), 0);
    EXPECT_FALSE(duplicate.accept().has_value());
}

TEST(Listener, DestructorReleasesTheBoundPort) {
    std::uint16_t port{};

    {
        networking::listener listener{ 0 };

        ASSERT_TRUE(listener.is_valid());
        port = listener.port();
    }

    networking::listener replacement{ port };

    EXPECT_TRUE(replacement.is_valid());
    EXPECT_EQ(replacement.port(), port);
}

TEST(Listener, MoveConstructionPreservesPortAndPendingConnection) {
    networking::listener original{ 0 };

    ASSERT_TRUE(original.is_valid());

    const auto port = original.port();
    auto peer = connect_tcp_peer(port);
    networking::listener moved{ std::move(original) };

    EXPECT_FALSE(original.is_valid());
    EXPECT_EQ(original.port(), 0);
    EXPECT_FALSE(original.accept().has_value());
    EXPECT_EQ(moved.port(), port);

    auto accepted = moved.accept();

    ASSERT_TRUE(accepted.has_value());

    const auto outgoing_bytes = vector_byte({ 1 });

    write_all_bytes(peer.get(), outgoing_bytes);

    const auto expected_bytes = vector_byte({ 1 });
    const auto received_bytes = accepted->read(1);

    EXPECT_EQ(received_bytes, expected_bytes);
}

TEST(Listener, MoveAssignmentReleasesPreviousPort) {
    networking::listener source{ 0 };
    networking::listener target{ 0 };

    ASSERT_TRUE(source.is_valid());
    ASSERT_TRUE(target.is_valid());

    const auto source_port = source.port();
    const auto old_port = target.port();

    EXPECT_EQ(&(target = std::move(source)), &target);
    EXPECT_FALSE(source.is_valid());
    EXPECT_EQ(target.port(), source_port);

    networking::listener replacement{ old_port };

    EXPECT_TRUE(replacement.is_valid());

    auto peer = connect_tcp_peer(source_port);

    EXPECT_TRUE(target.accept().has_value());
}

TEST(Listener, SelfMovePreservesListener) {
    networking::listener listener{ 0 };

    ASSERT_TRUE(listener.is_valid());

    const auto port = listener.port();
    auto& alias = listener;

    listener = std::move(alias);

    EXPECT_EQ(listener.port(), port);

    auto peer = connect_tcp_peer(port);

    EXPECT_TRUE(listener.accept().has_value());
}

TEST(Listener, MovingInvalidListenerInvalidatesDestination) {
    networking::listener first{ 0 };

    ASSERT_TRUE(first.is_valid());

    networking::listener owner{ std::move(first) };
    networking::listener target{ 0 };

    target = std::move(first);

    EXPECT_FALSE(target.is_valid());
    EXPECT_EQ(target.port(), 0);
    EXPECT_FALSE(target.accept().has_value());
}

TEST(Listener, TwoConnectionsDoNotMixTheirData) {
    networking::listener listener{ 0 };

    ASSERT_TRUE(listener.is_valid());

    auto first_peer = connect_tcp_peer(listener.port());
    auto first = listener.accept();
    auto second_peer = connect_tcp_peer(listener.port());
    auto second = listener.accept();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    const auto outgoing_bytes = vector_byte({ 1, 1 });

    write_all_bytes(first_peer.get(), outgoing_bytes);
    const auto outgoing_bytes_second = vector_byte({ 2, 2 });

    write_all_bytes(second_peer.get(), outgoing_bytes_second);

    const auto expected_bytes = vector_byte({ 2, 2 });
    const auto received_bytes = second->read(2);

    EXPECT_EQ(received_bytes, expected_bytes);
    const auto expected_bytes_second = vector_byte({ 1, 1 });
    const auto received_bytes_second = first->read(2);

    EXPECT_EQ(received_bytes_second, expected_bytes_second);
    const auto outgoing_bytes_third = vector_byte({ 3 });
    const auto bytes_written = first->write_all(outgoing_bytes_third);

    ASSERT_EQ(bytes_written, 1u);
    const auto outgoing_bytes_fourth = vector_byte({ 4 });
    const auto bytes_written_second = second->write_all(outgoing_bytes_fourth);

    ASSERT_EQ(bytes_written_second, 1u);
    const auto expected_bytes_third = vector_byte({ 4 });
    const auto received_bytes_third = read_exact_bytes(second_peer.get(), 1);

    EXPECT_EQ(received_bytes_third, expected_bytes_third);
    const auto expected_bytes_fourth = vector_byte({ 3 });
    const auto received_bytes_fourth = read_exact_bytes(first_peer.get(), 1);

    EXPECT_EQ(received_bytes_fourth, expected_bytes_fourth);
}

TEST(Listener, AcceptedSocketOutlivesListener) {
    std::optional<networking::socket> accepted{};
    unique_fd peer{};

    {
        networking::listener listener{ 0 };

        ASSERT_TRUE(listener.is_valid());
        peer = connect_tcp_peer(listener.port());
        accepted = listener.accept();

        ASSERT_TRUE(accepted.has_value());
    }

    const auto outgoing_bytes = vector_byte({ 42 });

    write_all_bytes(peer.get(), outgoing_bytes);

    const auto expected_bytes = vector_byte({ 42 });
    const auto received_bytes = accepted->read(1);

    EXPECT_EQ(received_bytes, expected_bytes);
}

} // namespace
