#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "io.hpp"
#include "common.hpp"
#include "debug.hpp"

#include <optional>
#include <type_traits>
#include <fstream>
#include <queue>
#include <vector>
#include <stdexcept>
#include <tuple>

namespace PPCB {
using namespace PPCB;

class File {
private:
    std::queue<Packet<DATA>> _packets;
    b_cnt_t _size;
public:
    File(session_t session_id) : _size(0) {
        p_cnt_t packet_number = 0;
        while (std::cin.peek() != EOF) {
            static std::vector<char> buffor(OPTIMAL_DATA_SIZE);

            std::cin.read(buffor.data(), OPTIMAL_DATA_SIZE);

            if (std::cin.gcount() == 0) {
                throw std::runtime_error("Failed to read stdin");
            }

            _packets.emplace(
                session_id, packet_number, std::cin.gcount(), buffor.data());

            packet_number++;
            _size += std::cin.gcount();
        }
    }

    b_cnt_t get_size() {
        return _size;
    }

    Packet<DATA> get_next_packet() {
        auto ret = _packets.front();
        _packets.pop();
        _size -= ret._packet_byte_cnt;
        return ret;
    }
};

// Function that reads next packet for given session
// and auto-respond (UDP) or throw exception (TCP) to other packets.
template <IO::Socket::connection_t C>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session(IO::Socket &socket, sockaddr_in client_address,
    session_t current_session_id, bool is_server,
    std::chrono::steady_clock::time_point to_begin = 
    std::chrono::steady_clock::now());

template <>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session<IO::Socket::UDP>(IO::Socket &socket,
    sockaddr_in client_address, session_t current_session_id, bool is_server,
    std::chrono::steady_clock::time_point to_begin) {

    while (true) {
        try {
        sockaddr_in addr;
        auto reader = std::make_unique<IO::PacketReader<IO::Socket::UDP>>
            (socket, &addr, true, to_begin);
        auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

        DBG_printer("readed next: id->", packet_to_string(id), 
            "session_id->", session_id);

        if (session_id == current_session_id && addr == client_address) {
            reader->mtb();
            return {std::move(reader), id};
        } else if (id == CONN && is_server) {
            Packet<CONNRJT>(session_id)
                .getSender(socket, &addr)
                .send<IO::Socket::UDP>();
        } else if (id == DATA && is_server) {
            Packet<DATA> data(*reader);
            Packet<RJT>(session_id, data._packet_number)
                .getSender(socket, &addr)
                .send<IO::Socket::UDP>();
        }
        } catch (IO::packet_smaller_than_expected &e) {
            // Incorrect packet, skipping
        }
    }
}

template <>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session<IO::Socket::TCP>(IO::Socket &socket, sockaddr_in addr, 
    session_t current_session_id, bool ,
    std::chrono::steady_clock::time_point to_begin) {

    auto reader = std::make_unique<IO::PacketReader<IO::Socket::TCP>>
        (socket, &addr, true, to_begin);

    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

    DBG_printer("readed next: id->", packet_to_string(id), 
        "session_id->", session_id);

    if (session_id != current_session_id) {
        throw std::runtime_error(
            std::string("Received unexptected session_id on tcp connection: ") +
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

// Helper type for function to get as many arguments as template arguments.
template<packet_type_t T>
using to_int = int;

template<packet_type_t P>
concept Orderedable = (std::derived_from<Packet<P>, PacketOrderedBase>);

template<packet_type_t P>
concept Unorderedable = (std::derived_from<Packet<P>, PacketBase> && 
    (!std::derived_from<Packet<P>, PacketOrderedBase>));

// Checks whether we can skip this packet (was already received).
template<packet_type_t P>
requires Orderedable<P>
bool can_skip(std::unique_ptr<IO::PacketReaderBase> &reader, p_cnt_t wanted_num) {
    reader->mtb();
    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

    if (id != P) {
        reader->mtb();
        return false;
    } else {
        auto [packet_number] = reader->readGeneric<p_cnt_t>();
        packet_number = to_host(packet_number);
        reader->mtb();
        return packet_number < wanted_num;
    }
}

// Checks whether we can skip this packet (was already received).
template<packet_type_t P>
requires Unorderedable<P>
bool can_skip(std::unique_ptr<IO::PacketReaderBase> &reader, p_cnt_t) {
    reader->mtb();
    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();
    reader->mtb();

    if (id == P) {
        return true;
    } else {
        return false;
    }
}

template<protocol_t P>
class Session {
private:
    IO::Socket &_socket;
    sockaddr_in _addr;
    session_t _session_id;
    std::unique_ptr<PacketBase> _last_msg;
    int _retransmit_cnt{0};
    // retransmit will not be used after reading second packet in a row (RCVD case)
    bool _retransmit_ready{false};
    bool _is_server;
    static constexpr IO::Socket::connection_t connection = 
                            (uses_tcp<P>() ? IO::Socket::TCP : IO::Socket::UDP);
public:
    Session(IO::Socket &socket, sockaddr_in addr, int64_t session_id, bool is_server) 
    : _socket(socket), _addr(addr), _session_id(session_id), _is_server(is_server) {}

    void send(std::unique_ptr<PacketBase> packet) {
        DBG_printer("sending: ", *packet);
        packet->getSender(_socket, &_addr).send<connection>();
        _retransmit_cnt = MAX_RETRANSMITS;
        _last_msg = std::move(packet);
        _retransmit_ready = true;
    }


    // Functions that pass next received packet to proccess in current session.
    template<packet_type_t... Ps>
    requires (!retransmits<P>())
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
        to_int<Ps>... , std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        return get_next_from_session<connection>(
            _socket, _addr, _session_id, _is_server, to_begin);
    }

    template<packet_type_t... Ps>
    requires (retransmits<P>())
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
        to_int<Ps>... cnts, std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        while (true) {
            try {
                auto [reader, id]  =  get_next_from_session<connection>
                    (_socket, _addr, _session_id, _is_server, to_begin);
                if ((can_skip<Ps>(reader, cnts) || ...)) {
                    continue;
                }

                reader->mtb();
                _retransmit_ready = false;
                return {std::move(reader), id};
            } catch (IO::timeout_error &e) {
                if (_retransmit_cnt <= 0 || !_retransmit_ready) {
                    throw e;
                }

                DBG_printer("retransmiting cnt->", _retransmit_cnt, 
                    "id->", packet_to_string(_last_msg->getID()));

                _retransmit_cnt--;
                _last_msg->getSender(_socket, &_addr).send<connection>();
                to_begin = std::chrono::steady_clock::now();
            }
        }
    }
};
} /* PPCB namespace */

#endif /* INTERFACE_HPP */