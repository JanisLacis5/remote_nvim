#include "test_support.hpp"
#include <fcntl.h>
#include <future>
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>

namespace
{
using namespace test_support;
using namespace std::chrono_literals;

TEST(SocketOwnership, IsMoveOnlyAndMovesWithoutThrowing) {
    EXPECT_FALSE(std::is_copy_constructible_v<networking::socket>);
    EXPECT_FALSE(std::is_copy_assignable_v<networking::socket>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<networking::socket>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<networking::socket>);
}

TEST(SocketConstruction, CreatesTcpAndUdpSockets) {
    networking::socket tcp{ AF_INET, SOCK_STREAM };
    networking::socket udp{ AF_INET, SOCK_DGRAM };

    EXPECT_TRUE(tcp.is_valid());
    EXPECT_TRUE(udp.is_valid());
}

TEST(SocketConstruction, InvalidFamilyAndDescriptorAreInvalid) {
    networking::socket bad_family{ -1, SOCK_STREAM };
    networking::socket bad_fd{ -1 };

    EXPECT_FALSE(bad_family.is_valid());
    EXPECT_FALSE(bad_fd.is_valid());
}

TEST(SocketConstruction, RejectsAndClosesAnAdoptedNonSocket) {
    const auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);

    ASSERT_GE(fd, 0);

    networking::socket socket{ fd };

    EXPECT_FALSE(socket.is_valid());
    const auto descriptor_result = ::fcntl(fd, F_GETFD);

    EXPECT_EQ(descriptor_result, -1);
    EXPECT_EQ(errno, EBADF);
}

TEST(SocketOwnership, DestructorClosesAdoptedDescriptor) {
    socket_pair pair{};
    const auto fd = pair.local.get();

    {
        const auto socket_fd = pair.local.release();
        networking::socket socket{ socket_fd };

        ASSERT_TRUE(socket.is_valid());
    }

    const auto descriptor_result = ::fcntl(fd, F_GETFD);

    EXPECT_EQ(descriptor_result, -1);
    EXPECT_EQ(errno, EBADF);

    std::byte byte{};

    const auto received_count = ::recv(pair.peer.get(), &byte, 1, 0);

    EXPECT_EQ(received_count, 0);
}

TEST(SocketOwnership, MoveConstructionTransfersTheConnection) {
    socket_pair pair{};
    const auto original_fd = pair.local.release();
    networking::socket original{ original_fd };
    networking::socket moved{ std::move(original) };

    EXPECT_FALSE(original.is_valid());
    EXPECT_TRUE(moved.is_valid());

    const auto payload = vector_byte({ 0, 128, 255 });

    const auto bytes_written = moved.write_all(payload);

    ASSERT_EQ(bytes_written, payload.size());
    const auto received_bytes = read_exact_bytes(pair.peer.get(), payload.size());

    EXPECT_EQ(received_bytes, payload);
    const auto bytes_written_second = original.write_all(payload);

    EXPECT_EQ(bytes_written_second, 0u);
    const auto received_bytes_second = original.read(1);

    EXPECT_TRUE(received_bytes_second.empty());
}

TEST(SocketOwnership, MoveAssignmentClosesOldConnectionAndInvalidatesSource) {
    socket_pair source_pair{};
    socket_pair target_pair{};
    const auto source_fd = source_pair.local.release();
    networking::socket source{ source_fd };
    const auto old_fd = target_pair.local.get();
    const auto target_fd = target_pair.local.release();
    networking::socket target{ target_fd };

    EXPECT_EQ(&(target = std::move(source)), &target);
    EXPECT_FALSE(source.is_valid());
    EXPECT_TRUE(target.is_valid());
    const auto descriptor_result = ::fcntl(old_fd, F_GETFD);

    EXPECT_EQ(descriptor_result, -1);

    std::byte byte{};

    const auto received_count = ::recv(target_pair.peer.get(), &byte, 1, 0);

    EXPECT_EQ(received_count, 0);

    const auto payload = vector_byte({ 42 });

    const auto bytes_written = target.write_all(payload);

    ASSERT_EQ(bytes_written, 1u);
    const auto received_bytes = read_exact_bytes(source_pair.peer.get(), 1);

    EXPECT_EQ(received_bytes, payload);
}

TEST(SocketOwnership, SelfMoveAssignmentKeepsTheConnection) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    auto& alias = socket;

    socket = std::move(alias);

    ASSERT_TRUE(socket.is_valid());

    const auto payload = vector_byte({ 42 });

    write_all_bytes(pair.peer.get(), payload);

    const auto received_bytes = socket.read(1);

    EXPECT_EQ(received_bytes, payload);
}

TEST(SocketOwnership, MovingInvalidSocketClosesDestination) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    networking::socket invalid{ -1 };

    socket = std::move(invalid);

    EXPECT_FALSE(socket.is_valid());
    EXPECT_FALSE(invalid.is_valid());

    networking::socket moved{ std::move(invalid) };

    EXPECT_FALSE(moved.is_valid());

    std::byte byte{};

    const auto received_count = ::recv(pair.peer.get(), &byte, 1, 0);

    EXPECT_EQ(received_count, 0);
}

TEST(SocketIo, InvalidSocketOperationsFailCleanly) {
    networking::socket socket{ -1 };

    const auto bind_result = socket.bind(INADDR_LOOPBACK, 0);

    EXPECT_FALSE(bind_result);
    const auto connected = socket.connect(INADDR_LOOPBACK, 0);

    EXPECT_FALSE(connected);
    const auto received_bytes = socket.read(1);

    EXPECT_TRUE(received_bytes.empty());
    const auto outgoing_bytes = vector_byte({ 1, 2, 3 });
    const auto bytes_written = socket.write_all(outgoing_bytes);

    EXPECT_EQ(bytes_written, 0u);
}

TEST(SocketIo, ZeroLengthOperationsDoNotConsumeQueuedData) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = vector_byte({ 1 });

    write_all_bytes(pair.peer.get(), payload);

    const auto received_bytes = socket.read(0);

    EXPECT_TRUE(received_bytes.empty());
    const auto bytes_written = socket.write_all({});

    EXPECT_EQ(bytes_written, 0u);
    const auto received_bytes_second = socket.read(1);

    EXPECT_EQ(received_bytes_second, payload);
}

class SocketReadSizes : public testing::TestWithParam<std::size_t>
{
};

TEST_P(SocketReadSizes, ReadsAllBytesFromRawPeer) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload_size = GetParam();
    const auto payload = make_byte_pattern(payload_size);

    write_all_bytes(pair.peer.get(), payload);

    const auto received_bytes = socket.read(payload.size());

    EXPECT_EQ(received_bytes, payload);
}

INSTANTIATE_TEST_SUITE_P(BufferBoundaries, SocketReadSizes,
                         testing::Values(1u, 255u, 1024u, 4095u, 4096u, 4097u, 16384u));

TEST(SocketIo, ReadsFragmentedPayloadWithoutDroppingEarlierChunks) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = make_byte_pattern(12000);

    const std::span payload_bytes{ payload };
    const auto first_chunk = payload_bytes.first(3);
    write_all_bytes(pair.peer.get(), first_chunk);

    auto writer = std::async(std::launch::async, [&] {
        std::this_thread::sleep_for(20ms);

        const auto middle_chunk = payload_bytes.subspan(3, 5000);
        write_all_bytes(pair.peer.get(), middle_chunk);

        std::this_thread::sleep_for(20ms);

        const auto final_chunk = payload_bytes.subspan(5003);
        write_all_bytes(pair.peer.get(), final_chunk);
    });

    const auto received_bytes = socket.read(payload.size());

    EXPECT_EQ(received_bytes, payload);
    EXPECT_NO_THROW(writer.get());
}

TEST(SocketIo, ReadConsumesOnlyRequestedBytes) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    const auto outgoing_bytes = vector_byte({ 1, 2, 3, 4, 5 });

    write_all_bytes(pair.peer.get(), outgoing_bytes);

    const auto expected_bytes = vector_byte({ 1, 2 });
    const auto received_bytes = socket.read(2);

    EXPECT_EQ(received_bytes, expected_bytes);
    const auto expected_bytes_second = vector_byte({ 3, 4, 5 });
    const auto received_bytes_second = socket.read(3);

    EXPECT_EQ(received_bytes_second, expected_bytes_second);
}

TEST(SocketIo, EofBeforeAnyDataReturnsEmpty) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    pair.peer.reset();

    const auto received_bytes = socket.read(1);

    EXPECT_TRUE(received_bytes.empty());
}

TEST(SocketIo, PartialPayloadFollowedByEofReturnsEmpty) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    const auto outgoing_bytes = vector_byte({ 1, 2, 3 });

    write_all_bytes(pair.peer.get(), outgoing_bytes);

    const auto shutdown_result = ::shutdown(pair.peer.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);
    const auto received_bytes = socket.read(4);

    EXPECT_TRUE(received_bytes.empty());
}

TEST(SocketIo, CompletePayloadBeforeEofIsReturned) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = vector_byte({ 0, 255, 1 });

    write_all_bytes(pair.peer.get(), payload);

    const auto shutdown_result = ::shutdown(pair.peer.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);
    const auto received_bytes = socket.read(payload.size());

    EXPECT_EQ(received_bytes, payload);
    const auto received_bytes_second = socket.read(1);

    EXPECT_TRUE(received_bytes_second.empty());
}

TEST(SocketIo, NonblockingReadWithoutDataReturnsEmpty) {
    socket_pair pair{};
    const auto fd = pair.local.get();

    const auto original_flags = ::fcntl(fd, F_GETFL);
    const auto descriptor_result = ::fcntl(fd, F_SETFL, original_flags | O_NONBLOCK);

    ASSERT_EQ(descriptor_result, 0);

    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    const auto received_bytes = socket.read(1);

    EXPECT_TRUE(received_bytes.empty());
}

TEST(SocketIo, RawPeerReceivesExactlyTheWrittenBinaryBytes) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = make_byte_pattern(16384);

    const auto bytes_written = socket.write_all(payload);

    ASSERT_EQ(bytes_written, payload.size());
    const auto received_bytes = read_exact_bytes(pair.peer.get(), payload.size());

    EXPECT_EQ(received_bytes, payload);
}

TEST(SocketIo, LargeWriteWithSmallSendBufferHandlesBackpressure) {
    socket_pair pair{};
    const int size{ 4096 };

    const auto option_result = ::setsockopt(pair.local.get(), SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));

    ASSERT_EQ(option_result, 0);

    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = make_byte_pattern(1024 * 1024);
    auto reader = std::async(std::launch::async, [&] { return read_exact_bytes(pair.peer.get(), payload.size()); });

    const auto bytes_written = socket.write_all(payload);

    EXPECT_EQ(bytes_written, payload.size());
    EXPECT_EQ(reader.get(), payload);
}

TEST(SocketIo, NonblockingPartialWriteReportsOnlyBytesActuallySent) {
    socket_pair pair{};
    const auto fd = pair.local.get();
    const int size{ 4096 };

    const auto option_result = ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));

    ASSERT_EQ(option_result, 0);
    const auto original_flags = ::fcntl(fd, F_GETFL);
    const auto descriptor_result = ::fcntl(fd, F_SETFL, original_flags | O_NONBLOCK);

    ASSERT_EQ(descriptor_result, 0);

    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };
    const auto payload = make_byte_pattern(1024 * 1024);
    const auto sent = socket.write_all(payload);

    ASSERT_GT(sent, 0u);
    ASSERT_LT(sent, payload.size());
    const auto bytes_written = socket.write_all(payload);

    EXPECT_EQ(bytes_written, 0u); // full buffer: initial EAGAIN
    const auto received_bytes = read_exact_bytes(pair.peer.get(), sent);

    const std::vector<std::byte> expected_prefix{ payload.begin(), payload.begin() + sent };
    EXPECT_EQ(received_bytes, expected_prefix);
}

TEST(SocketIo, ClosedPeerReturnsZeroWithoutSigpipeOrUnsignedWraparound) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    pair.peer.reset();

    const auto outgoing_bytes = vector_byte({ 1, 2, 3 });
    const auto bytes_written = socket.write_all(outgoing_bytes);

    EXPECT_EQ(bytes_written, 0u);
}

TEST(SocketIo, LocallyShutdownWriterReturnsZero) {
    socket_pair pair{};

    const auto shutdown_result = ::shutdown(pair.local.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);

    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    const auto outgoing_bytes = vector_byte({ 1 });
    const auto bytes_written = socket.write_all(outgoing_bytes);

    EXPECT_EQ(bytes_written, 0u);
}

TEST(SocketIo, ReadSideEofStillAllowsWritingBack) {
    socket_pair pair{};
    const auto socket_fd = pair.local.release();
    networking::socket socket{ socket_fd };

    const auto shutdown_result = ::shutdown(pair.peer.get(), SHUT_WR);

    ASSERT_EQ(shutdown_result, 0);
    const auto received_bytes = socket.read(1);

    EXPECT_TRUE(received_bytes.empty());
    const auto outgoing_bytes = vector_byte({ 42 });
    const auto bytes_written = socket.write_all(outgoing_bytes);

    ASSERT_EQ(bytes_written, 1u);
    const auto expected_bytes = vector_byte({ 42 });
    const auto received_bytes_second = read_exact_bytes(pair.peer.get(), 1);

    EXPECT_EQ(received_bytes_second, expected_bytes);
}

TEST(SocketBind, BindsHostOrderLoopbackAddressAndEphemeralPort) {
    const auto fd_value = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unique_fd fd{ fd_value };

    ASSERT_GE(fd.get(), 0);

    const auto raw = fd.get();
    networking::socket socket{ fd.release() };

    const auto bind_result = socket.bind(INADDR_LOOPBACK, 0);

    ASSERT_TRUE(bind_result);
    sockaddr_in address{};
    socklen_t size{ sizeof(address) };

    const auto address_result = ::getsockname(raw, reinterpret_cast<sockaddr*>(&address), &size);

    ASSERT_EQ(address_result, 0);
    EXPECT_EQ(ntohl(address.sin_addr.s_addr), INADDR_LOOPBACK);
    EXPECT_NE(ntohs(address.sin_port), 0);
    const auto bind_result_second = socket.bind(INADDR_LOOPBACK, 0);

    EXPECT_FALSE(bind_result_second);
}

TEST(SocketBind, DuplicateAddressFails) {
    const auto fd_value = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unique_fd fd{ fd_value };

    ASSERT_GE(fd.get(), 0);

    const auto raw = fd.get();
    networking::socket first{ fd.release() };

    const auto bind_result = first.bind(INADDR_LOOPBACK, 0);

    ASSERT_TRUE(bind_result);
    sockaddr_in address{};
    socklen_t size{ sizeof(address) };

    const auto address_result = ::getsockname(raw, reinterpret_cast<sockaddr*>(&address), &size);

    ASSERT_EQ(address_result, 0);

    networking::socket second{ AF_INET, SOCK_STREAM };

    const auto bind_result_second = second.bind(INADDR_LOOPBACK, ntohs(address.sin_port));

    EXPECT_FALSE(bind_result_second);
}

TEST(SocketConnect, ConnectionRefusedReturnsFalse) {
    // Keep a port bound but not listening, avoiding a free-port allocation race.
    const auto reserved_value = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unique_fd reserved{ reserved_value };

    ASSERT_GE(reserved.get(), 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const auto bind_result = ::bind(reserved.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address));

    ASSERT_EQ(bind_result, 0);

    socklen_t size{ sizeof(address) };

    const auto address_result = ::getsockname(reserved.get(), reinterpret_cast<sockaddr*>(&address), &size);

    ASSERT_EQ(address_result, 0);

    networking::socket socket{ AF_INET, SOCK_STREAM };

    const auto connected = socket.connect(INADDR_LOOPBACK, ntohs(address.sin_port));

    EXPECT_FALSE(connected);
}

} // namespace
