// Loopback harness for runtime/src/network_transport.cpp: a local fake server speaks the
// historical stream (VLQ(length)+init payload, then VLQ(id)+VLQ(length)+payload frames)
// and checks what the transport queues for the guest and what it sends back.
#include "multiplayer_protocol.h"
#include "network_transport.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
static void close_socket(Socket s) { closesocket(s); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
static void close_socket(Socket s) { close(s); }
#endif

using namespace multiplayer;
using Bytes = std::vector<std::uint8_t>;

namespace {

struct Listener {
    Socket socket;
    std::uint16_t port;
};

Listener listen_loopback() {
    Socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    assert(bind(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    assert(listen(s, 1) == 0);
    socklen_t size = sizeof(address);
    assert(getsockname(s, reinterpret_cast<sockaddr*>(&address), &size) == 0);
    return {s, ntohs(address.sin_port)};
}

void send_bytes(Socket s, const Bytes& data, bool one_byte_at_a_time) {
    if (!one_byte_at_a_time) {
        assert(send(s, reinterpret_cast<const char*>(data.data()), int(data.size()), 0) ==
               int(data.size()));
        return;
    }
    for (std::uint8_t byte : data) {
        assert(send(s, reinterpret_cast<const char*>(&byte), 1, 0) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

Bytes receive_bytes(Socket s, std::size_t size) {
    Bytes data(size);
    std::size_t done = 0;
    while (done != size) {
        const int got = recv(s, reinterpret_cast<char*>(data.data() + done), int(size - done), 0);
        assert(got > 0);
        done += std::size_t(got);
    }
    return data;
}

template <class Predicate>
bool wait_for(Predicate predicate) {
    for (int i = 0; i != 300; ++i) {
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

Bytes poll_item(NetworkTransport& transport) {
    Bytes buffer(2 * 1024 * 1024);
    std::size_t size = 0;
    wait_for([&] { return (size = transport.receive(buffer.data(), buffer.size())) != 0; });
    buffer.resize(size);
    return buffer;
}

Bytes frame(std::uint32_t id, const Bytes& payload) {
    Writer writer;
    assert(write_frame(writer, id, payload));
    return writer.data();
}

// Init payload shaped like Client::readInitPacket expects: receiver names, sender names
// (index = packet ID), then an empty initializer list.
Bytes init_blob() {
    Writer payload;
    payload.vlq(1);
    payload.string("SERVER_SAVE_UPDATE");
    payload.vlq(3);
    payload.string("CLIENT_STUB");
    payload.string("CLIENT_TEST");
    payload.string("CLIENT_INIT_PACKET");
    payload.vlq(0);
    Writer blob;
    blob.vlq(std::uint32_t(payload.data().size()));
    blob.bytes(payload.data().data(), payload.data().size());
    return blob.data();
}

void test_handshake_and_frames() {
    Listener listener = listen_loopback();
    NetworkTransport transport;
    assert(transport.connect("127.0.0.1", listener.port));
    Socket server = accept(listener.socket, nullptr, nullptr);
    assert(server != Socket(-1));
    assert(wait_for([&] { return transport.status() == TransportStatus::connected; }));

    // Init blob split byte by byte, then two frames coalesced in a single write.
    const Bytes init = init_blob();
    send_bytes(server, init, true);
    Bytes coalesced = frame(0, {0x01, 0x02, 0x03});
    const Bytes second = frame(200, {});
    coalesced.insert(coalesced.end(), second.begin(), second.end());
    send_bytes(server, coalesced, false);

    assert(poll_item(transport) == init);
    assert(poll_item(transport) == frame(0, {0x01, 0x02, 0x03}));
    assert(poll_item(transport) == frame(200, {}));

    // A client frame reaches the server byte for byte (CLIENT_INIT_PACKET = ID 2, 8 bytes).
    const Bytes outgoing = frame(2, {'w', 'w', 'h', 'd', 'u', 's', 'e', 'r'});
    assert(transport.send(outgoing.data(), outgoing.size()));
    assert(receive_bytes(server, outgoing.size()) == outgoing);

    // A too small guest buffer leaves the item queued instead of truncating it.
    const Bytes big = frame(1, Bytes(64, 0xAB));
    send_bytes(server, big, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::uint8_t small[16];
    assert(transport.receive(small, sizeof(small)) == 0);
    assert(poll_item(transport) == big);

    // Server close is reported as an orderly disconnection.
    close_socket(server);
    assert(wait_for([&] { return transport.status() == TransportStatus::disconnected; }));
    assert(transport.last_error() == 0);
    assert(!transport.send(outgoing.data(), outgoing.size()));

    transport.disconnect();
    close_socket(listener.socket);
}

void test_oversized_frame_rejected_from_header() {
    Listener listener = listen_loopback();
    NetworkTransport transport;
    assert(transport.connect("127.0.0.1", listener.port));
    Socket server = accept(listener.socket, nullptr, nullptr);
    send_bytes(server, init_blob(), false);

    // Only the header is sent: rejection must not wait for 1 MiB of payload.
    Writer header;
    header.vlq(0);
    header.vlq(1024 * 1024 + 1);
    send_bytes(server, header.data(), false);
    assert(wait_for([&] { return transport.status() == TransportStatus::error; }));

    transport.disconnect();
    close_socket(server);
    close_socket(listener.socket);
}

void test_disconnect_while_connected_and_refused_port() {
    Listener listener = listen_loopback();
    NetworkTransport transport;
    assert(transport.connect("127.0.0.1", listener.port));
    Socket server = accept(listener.socket, nullptr, nullptr);
    assert(wait_for([&] { return transport.status() == TransportStatus::connected; }));
    transport.disconnect();
    assert(transport.status() == TransportStatus::disconnected);
    std::uint8_t byte = 0;
    assert(recv(server, reinterpret_cast<char*>(&byte), 1, 0) <= 0);
    close_socket(server);

    // Nothing listens on the port any more: the failure is an explicit error state.
    const std::uint16_t port = listener.port;
    close_socket(listener.socket);
    assert(transport.connect("127.0.0.1", port));
    assert(wait_for([&] { return transport.status() == TransportStatus::error; }));
    assert(transport.last_error() != 0);
    transport.disconnect();

    assert(!transport.connect("", 10014));
    assert(!transport.connect("127.0.0.1", 0));
}

} // namespace

int main() {
#ifdef _WIN32
    WSADATA data = {};
    assert(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    test_handshake_and_frames();
    test_oversized_frame_rejected_from_header();
    test_disconnect_while_connected_and_refused_port();
    std::puts("network_transport_test: ok");
    return 0;
}
