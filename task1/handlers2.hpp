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
    session_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin) {

    auto reader = std::make_unique<IO::PacketReader<IO::Socket::TCP>>
        (socket, &addr, true, to_begin);

    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

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

template<packet_type_t T>
using to_int = int;

template<packet_type_t P>
concept Orderedable = (std::derived_from<Packet<P>, PacketOrderedBase>);

template<packet_type_t P>
concept Unorderedable = (
    std::derived_from<Packet<P>, PacketBase> && 
    (!std::derived_from<Packet<P>, PacketOrderedBase>));

template<packet_type_t P>
requires Orderedable<P>
bool can_skip(std::unique_ptr<IO::PacketReaderBase> &reader, p_cnt_t wanted_num) {
    auto [id, session_id] = reader->readGeneric<packet_type_t, session_t>();

    if (id != P) {
        reader->mtb();
        return false;
    } else {
        auto [packet_number] = reader->readGeneric<p_cnt_t>();
        reader->mtb();
        Packet<P> to_remove(*reader);
        return wanted_num < packet_number;
    }
}

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
        DBG_printer("sending: id->", packet_to_string(packet->getID()));
        packet->send(_socket, &_addr);
        _retransmit_cnt = MAX_RETRANSMITS;
        _last_msg = std::move(packet);
    }

    // template<packet_type_t... Ps>
    // std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> get_next(
    //     to_int<Ps>... cnts, std::chrono::steady_clock::time_point to_begin = 
    //         std::chrono::steady_clock::now());

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
                return {std::move(reader), id};
            } catch (IO::timeout_error &e) {
                if (_retransmit_cnt <= 0) {
                    throw e;
                }

                _retransmit_cnt--;
                _last_msg->send(_socket, &_addr);
                to_begin = std::chrono::steady_clock::now();
            }
        }
    }
};



template <protocol_t P>
void server_handler(Session<P> &session, Packet<CONN> conn) {
    try {
        session_t session_id = conn._session_id;
        b_cnt_t bytes_left = conn._data_len;

        session.send(
            std::make_unique<Packet<CONNACC>>(Packet<CONNACC>(session_id)));

        // std::vector<ASIO::Packet<DATA>> data;
        p_cnt_t packet_number = 0;

        while (bytes_left > 0) {
            auto [reader, packet_id] =
                session.template get_next<ACC, DATA>(0, packet_number);

            if (packet_id == DATA) {
                auto data_packet = Packet<DATA>::read(*reader);

                if (data_packet._packet_number != packet_number) {
                    auto ttt = std::make_unique<Packet<RJT>>
                            (Packet<RJT>(session_id,
                                         data_packet._packet_number));
                    session.send(std::move(ttt));
                        

                    throw std::runtime_error(
                        "Data packets not in order, expected:" +
                        std::to_string(packet_number) + ", received:" +
                        std::to_string(data_packet._packet_number));
                } else if (bytes_left < data_packet._packet_byte_cnt) {
                    session.send(
                        std::make_unique<Packet<RJT>>
                            (Packet<RJT>(session_id,
                                         data_packet._packet_number)));

                    throw std::runtime_error(
                        "Received to much bytes: expected:" +
                        std::to_string(conn._data_len) + ", received:" +
                        std::to_string(conn._data_len - bytes_left));
                }

                std::cout.write(
                    data_packet._data.data(), data_packet._data.size());
                std::cout << std::flush << "\n";


                bytes_left -= data_packet._packet_byte_cnt;
                packet_number++;

                if constexpr (retransmits<P>()) {
                    session.send(
                        std::make_unique<Packet<ACC>>
                            (Packet<ACC>(session_id,
                                         data_packet._packet_number)));
                }
            } else {
                throw unexpected_packet(DATA, packet_id);
            }
        }

        session.send(std::make_unique<Packet<RCVD>>(Packet<RCVD>(session_id)));

    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what();
    }
}



template <protocol_t P>
void client_handler(
    Session<P> &session, int64_t session_id, std::ifstream file, size_t file_size) {
    DBG_printer("Sending file of size: ", file_size);

    session.send(std::make_unique<Packet<CONN>>(
        Packet<CONN>(session_id, P, file_size)));

    auto [reader, id] = session.get_next();

    if (id == CONNRJT) {
        return;
    }

    if (id != CONNACC) {
        throw unexpected_packet(CONNACC, id);
    }

    Packet<CONNACC>::read(*reader);

    p_cnt_t packet_number = 0;
    while (file.peek() != EOF) {
        if constexpr (retransmits<P>()) {
            if (packet_number != 0) {
                auto [reader_2, id_2] = 
                    session.template get_next<CONN, ACC>(0, packet_number - 1);
                if (id_2 == ACC) {
                    auto packet = Packet<ACC>(reader_2);
                    if (packet._packet_number != packet_number - 1) {
                        throw unexpected_packet(ACC, ACC);
                    }
                } else if (id_2 == RJT) {
                    auto packet = Packet<RJT>(reader_2);
                    if (packet._packet_number != packet_number - 1) {
                        throw unexpected_packet(RJT, RJT);
                    } else {
                        throw rejected_data(packet_number - 1);
                    }
                } else {
                    throw unexpected_packet(RJT, RJT);
                }
            }
        }

        static std::vector<char> buffor(IO::MAX_DATA_SIZE);

        file.read(buffor.data(), IO::MAX_DATA_SIZE);
        Packet<DATA> data(
            session_id, packet_number, file.gcount(), buffor.data());

        session.send(std::make_unique<Packet<DATA>>(data));
        packet_number++;
    }

    auto [reader_2, id_2] = 
        session.template get_next<CONN, ACC>(0, packet_number);

    if (id_2 == RCVD) {
        return;
    } else if (id_2 == RJT) {
        throw std::runtime_error("Data packet rejected");
    } else {
        throw unexpected_packet(RCVD, id);
    }
}
}

#endif /* SERVER_HPP */