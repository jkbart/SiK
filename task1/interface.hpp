#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "io.hpp"
#include "common.hpp"
#include "debug.hpp"

#include <optional>
#include <type_traits>
#include <fstream>
#include <thread>
#include <queue>

namespace PPCB {
using namespace PPCB;

class File {
private:
    std::queue<Packet<DATA>> _packets;
    b_cnt_t _size;
public:
    File(const char* filename, session_t session_id) : _size(0) {
        std::ifstream file(filename, std::ifstream::binary);
        if (!file.is_open()) {
            throw std::runtime_error(
                "Failed to open file: " + std::string(filename));
        }

        p_cnt_t packet_number = 0;
        while (file.peek() != EOF) {
            static std::vector<char> buffor(IO::MAX_DATA_SIZE);

            file.read(buffor.data(), IO::MAX_DATA_SIZE);

            if (file.gcount() == 0) {
                throw std::runtime_error(
                    "Failed to open file: " + std::string(filename));
            }
            _packets.emplace(
                session_id, packet_number, file.gcount(), buffor.data());

            packet_number++;
            _size += file.gcount();
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

// Functions that read next packet for given session
// and auto-respond (UDP) or throw exception (TCP) to other packets.
template <IO::Socket::connection_t C>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session(IO::Socket &socket, sockaddr_in client_address,
    session_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin = 
    std::chrono::steady_clock::now());

template <>
std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
get_next_from_session<IO::Socket::UDP>(IO::Socket &socket,
    sockaddr_in client_address, session_t current_session_id,
    std::chrono::steady_clock::time_point to_begin) {

    while (true) {
        sockaddr_in addr;
        auto reader = std::make_unique<IO::PacketReader<IO::Socket::UDP>>
            (socket, &addr, true, to_begin);
        auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

        DBG_printer("readed next: id->", packet_to_string(id), 
            " ,session_id->", session_id);

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
    session_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin) {

    auto reader = std::make_unique<IO::PacketReader<IO::Socket::TCP>>
        (socket, &addr, true, to_begin);

    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

    DBG_printer("readed next: id->", packet_to_string(id), 
        " ,session_id->", session_id);

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
    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

    if (id != P) {
        reader->mtb();
        return false;
    } else {
        auto [packet_number] = reader->readGeneric<p_cnt_t>();
        packet_number = to_host(packet_number);
        reader->mtb();
        Packet<P> to_remove(*reader);
        return packet_number < wanted_num;
    }
}

// Checks whether we can skip this packet (was already received).
template<packet_type_t P>
requires Unorderedable<P>
bool can_skip(std::unique_ptr<IO::PacketReaderBase> &reader, p_cnt_t) {
    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();
    reader->mtb();

    if (id == P) {
        Packet<P> to_remove(*reader);
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
    int _retransmit_cnt;
    static constexpr IO::Socket::connection_t connection = 
                            (uses_tcp<P>() ? IO::Socket::TCP : IO::Socket::UDP);
public:
    Session(IO::Socket &socket, sockaddr_in addr, int64_t session_id) 
    : _socket(socket), _addr(addr), _session_id(session_id) {}

    void send(std::unique_ptr<PacketBase> packet) {
        DBG_printer("sending: ", *packet);
        packet->send(_socket, &_addr);
        _retransmit_cnt = MAX_RETRANSMITS;
        _last_msg = std::move(packet);
    }


    // Functions that pass next received packet to proccess in current session.
    template<packet_type_t... Ps>
    requires (!retransmits<P>())
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
        to_int<Ps>... , std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        return get_next_from_session<connection>(
            _socket, _addr, _session_id, to_begin);
    }

    template<packet_type_t... Ps>
    requires (retransmits<P>())
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
        to_int<Ps>... cnts, std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        while (true) {
            try {
                auto [reader, id]  =  get_next_from_session<connection>
                    (_socket, _addr, _session_id, to_begin);
                if ((can_skip<Ps>(reader, cnts) || ...)) {
                    continue;
                }

                reader->mtb();
                return {std::move(reader), id};
            } catch (IO::timeout_error &e) {
                if (_retransmit_cnt <= 0) {
                    throw e;
                }

                DBG_printer("retransmiting cnt->", _retransmit_cnt, 
                    "id->", packet_to_string(_last_msg->getID()));

                _retransmit_cnt--;
                _last_msg->send(_socket, &_addr);
                to_begin = std::chrono::steady_clock::now();
            }
        }
    }
};

template <protocol_t P>
void server_handler(Session<P> &session, Packet<CONN> conn) {
    session_t session_id = conn._session_id;
    b_cnt_t bytes_left = conn._data_len;

    session.send(
        std::make_unique<Packet<CONNACC>>(session_id));

    p_cnt_t packet_number = 0;

    while (bytes_left > 0) {
        auto [reader, packet_id] =
            session.template get_next<CONN, DATA>(0, packet_number);

        if (packet_id == DATA) {
            Packet<DATA> data_packet(*reader);

            if (data_packet._packet_number != packet_number) {
                session.send(std::make_unique<Packet<RJT>>
                        (session_id, data_packet._packet_number));

                // throw std::runtime_error(
                //     "Data packets not in order, expected:" +
                //     std::to_string(packet_number) + ", received:" +
                //     std::to_string(data_packet._packet_number));
                throw unexpected_packet(DATA, packet_number, 
                    DATA, data_packet._packet_number);
            } else if (bytes_left < data_packet._packet_byte_cnt) {
                session.send(
                    std::make_unique<Packet<RJT>>
                        (session_id, data_packet._packet_number));

                throw std::runtime_error(
                    "Received to much bytes: left to read:" +
                    std::to_string(conn._data_len) + ", received:" +
                    std::to_string(conn._data_len - bytes_left));
            }

            std::cout.write(
                data_packet._data.data(), data_packet._data.size());
            std::cout << std::flush;

            bytes_left -= data_packet._packet_byte_cnt;
            packet_number++;

            if constexpr (retransmits<P>()) {
                if (bytes_left != 0) {
                    session.send(
                        std::make_unique<Packet<ACC>>
                            (session_id, data_packet._packet_number));
                }
            }
        } else {
            throw unexpected_packet(DATA, std::nullopt, 
                packet_id, std::nullopt);
        }
    }

    session.send(std::make_unique<Packet<RCVD>>(session_id));
}



template <protocol_t P>
void client_handler(Session<P> &session, int64_t session_id, File &file) {
    DBG_printer("Sending file of size: ", file.get_size());

    session.send(
        std::make_unique<Packet<CONN>>(session_id, P, file.get_size()));

    auto [reader, id] = session.get_next();

    if (id == CONNRJT) {
        return;
    }

    if (id != CONNACC) {
        throw unexpected_packet(CONNACC, std::nullopt, id, std::nullopt);
    }

    Packet<CONNACC> connacc(*reader);

    p_cnt_t packet_number = 0;
    while (file.get_size() != 0) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 5));
        if constexpr (retransmits<P>()) {
            if (packet_number != 0) {
                auto [reader_2, id_2] = 
                    session.template get_next<CONNACC, ACC>
                        (0, packet_number - 1);
                if (id_2 == ACC) {
                    Packet<ACC> acc(*reader_2);
                    if (acc._packet_number != packet_number - 1) {
                        throw unexpected_packet(ACC, packet_number - 1,
                            ACC, acc._packet_number);
                    }
                } else if (id_2 == RJT) {
                    Packet<RJT> rjt(*reader_2);
                    if (rjt._packet_number != packet_number - 1) {
                        throw unexpected_packet(RJT, packet_number - 1,
                            RJT, rjt._packet_number);
                    } else {
                        throw rejected_data(packet_number - 1);
                    }
                } else {
                    throw unexpected_packet(RJT, std::nullopt, 
                        id_2, std::nullopt);
                }
            }
        }

        static std::vector<char> buffor(IO::MAX_DATA_SIZE);

        session.send(std::make_unique<Packet<DATA>>(file.get_next_packet()));
        packet_number++;
    }

    auto [reader_2, id_2] = 
        session.template get_next<CONNACC, ACC>(0, packet_number);

    if (id_2 == RCVD) {
        return;
    } else if (id_2 == RJT) {
        throw std::runtime_error("Data packet rejected");
    } else {
        throw unexpected_packet(RCVD, std::nullopt, id, std::nullopt);
    }
}
}

#endif /* INTERFACE_HPP */