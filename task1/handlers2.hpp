#ifndef SERVER_HPP
#define SERVER_HPP

#include "io.hpp"
#include "common.hpp"
#include "debug.hpp"
// #include "interface.hpp"

#include <optional>
#include <type_traits>
#include <fstream>

namespace ASIO {
using namespace ASIO;

template <IO::Socket::connection_t C>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session(IO::Socket &socket, sockaddr_in client_address,
    uint64_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin = 
    std::chrono::steady_clock::now());

template <>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session<IO::Socket::UDP>(IO::Socket &socket,
    sockaddr_in client_address, uint64_t current_session_id,
    std::chrono::steady_clock::time_point to_begin) {

    while (true) {
        sockaddr_in addr;
        auto reader = std::make_unique<IO::PacketReader<IO::Socket::UDP>>
            (socket, &addr, true, to_begin);
        auto [id, session_id] = reader->readGeneric<packet_type_t, uint64_t>();

        DBG_printer("readed next: id->", packet_to_string(id), " ,session_id->", session_id);

        if (session_id == current_session_id && addr == client_address) {
            reader->mtb();
            return {std::move(reader), id};
        } else if (id == CONN) {
            Packet<CONNRJT>(session_id).send(socket, &addr);
        }
    }
}

template <>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session<IO::Socket::TCP>(IO::Socket &socket, sockaddr_in addr, 
    uint64_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin) {

    auto reader = std::make_unique<IO::PacketReader<IO::Socket::TCP>>
        (socket, &addr, true, to_begin);

    auto [id, session_id] = reader->readGeneric<packet_type_t, uint64_t>();

    DBG_printer("readed next: id->", packet_to_string(id), " ,session_id->", session_id);

    if (session_id != current_session_id) {
        throw std::runtime_error(
            std::string("Received unexptected session_id: ") +
            std::string("expected: ") + std::to_string(current_session_id) +
            std::string("received: ") + std::to_string(session_id));
    }

    reader->mtb();
    return {std::move(reader), id};
}

template<protocol_t P>
constexpr bool retransmits() {
    return (P == udpr);
}

template<protocol_t P>
constexpr bool uses_tcp() {
    return (P == tcp);
}

template<protocol_t P>
constexpr bool uses_udp() {
    return (P == udp || P == udpr);
}

template<protocol_t P>
class Session {
private:
    IO::Socket &_socket;
    sockaddr_in _addr;
    uint64_t _session_id;
    static constexpr IO::Socket::connection_t connection = 
                            (uses_tcp<P>() ? IO::Socket::TCP : IO::Socket::UDP);
public:
    Session(IO::Socket &socket, sockaddr_in addr, int64_t session_id) 
    : _socket(socket), _addr(addr), _session_id(session_id) {}

    void send(std::unique_ptr<PacketBase> packet) {
        DBG_printer("sending: id->", packet_to_string(packet->getID()));
        packet->send(_socket, &_addr);
    }

    template<packet_type_t... Ordered, packet_type_t... Unordered>
    requires ((std::derived_from<Packet<Ordered>, PacketOrderedBase>, ...) && 
        ((std::derived_from<Packet<Unordered>, PacketBase> && 
        !std::derived_from<Packet<Unordered>, PacketOrderedBase>), ...))
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
        std::pair<Packet<Ordered>*, int>... is, // trick to get int for every Ordered value. Pointer is ignored so can be called with NULL.
        std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        return get_next_from_session<connection>(
            _socket, _addr, _session_id, to_begin);
    }
    // template<class... Args>
    // void get_next(std::pair<Args*, int>... is) {// trick to get int for every Ordered value. Pointer is ignored so can be called with NULL.
    //     ((std::cout << typeid(Args).name() << " " << is << "\n"),...);
    // }
};
}

#endif /* SERVER_HPP */