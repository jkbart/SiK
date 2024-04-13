#ifndef SERVER_HPP
#define SERVER_HPP

#include "io.hpp"
#include "common.hpp"
// #include "interface.hpp"

#include <optional>
#include <type_traits>
#include <fstream>

namespace ASIO {
using namespace ASIO;

template <IO::Socket::connection_t C>
std::tuple<IO::PacketReader<C>, packet_type_t>
get_next_from_session(IO::Socket &socket, sockaddr_in client_address,
    uint64_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin = 
    std::chrono::steady_clock::now());

template <>
std::tuple<IO::PacketReader<IO::Socket::UDP>, packet_type_t>
get_next_from_session<IO::Socket::UDP>(IO::Socket &socket,
    sockaddr_in client_address, uint64_t current_session_id,
    std::chrono::steady_clock::time_point to_begin) {

    while (true) {
        sockaddr_in addr;
        IO::PacketReader<IO::Socket::UDP> reader(socket, &addr, true, to_begin);
        auto [id, session_id] = reader.readGeneric<packet_type_t, uint64_t>();

        if (session_id == current_session_id && addr == client_address) {
            return {reader, id};
        } else if (id == CONN) {
            Packet<CONNRJT>(session_id).send(socket, &addr);
        }
    }
}

template <>
std::tuple<IO::PacketReader<IO::Socket::TCP>, packet_type_t>
get_next_from_session<IO::Socket::TCP>(IO::Socket &socket, sockaddr_in, 
    uint64_t current_session_id, 
    std::chrono::steady_clock::time_point to_begin) {

    IO::PacketReader<IO::Socket::TCP> reader(socket, NULL, true, to_begin);
    auto [id, session_id] = reader.readGeneric<packet_type_t, uint64_t>();

    std::cout << "readed next: id->" << packet_to_string(id) << " ,session_id->" << session_id << "\n";

    if (session_id != current_session_id) {
        throw std::runtime_error(
            std::string("Received unexptected session_id: ") +
            std::string("expected: ") + std::to_string(current_session_id) +
            std::string("received: ") + std::to_string(session_id));
    }

    return {reader, id};
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

class SessionBase {
protected:
    IO::Socket &_socket;
    sockaddr_in _addr;
    uint64_t _session_id;
public:
    SessionBase(IO::Socket &socket, sockaddr_in addr, int64_t session_id) 
    : _socket(socket), _addr(addr), _session_id(session_id) {}

    virtual void send(std::unique_ptr<PacketBase> packet) = 0;
    virtual std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t> 
        get_next() = 0;
};

template<protocol_t P>
class Session;

template<protocol_t P>
    requires (!retransmits<P>())
class Session<P> : SessionBase {
private:
    static constexpr IO::Socket::connection_t connection = 
                            (uses_tcp<P>() ? IO::Socket::TCP : IO::Socket::UDP);
public:
    Session(IO::Socket &socket, sockaddr_in addr, int64_t session_id) 
    : SessionBase(socket, addr, session_id) {}


    void send(std::unique_ptr<PacketBase> packet) {
        packet->send(_socket, &_addr);
    }

    std::tuple<IO::PacketReader<connection>, packet_type_t> get_next() {
        return get_next_from_session<connection>(_socket, _addr, _session_id);
    }
};

template<protocol_t P>
    requires (retransmits<P>())
class Session<P> : SessionBase {
private:
    std::unique_ptr<PacketBase> _last_msg;
    int _retransmit_cnt;

    static constexpr IO::Socket::connection_t connection = 
                            (uses_tcp<P>() ? IO::Socket::TCP : IO::Socket::UDP);
public:
    Session(IO::Socket &socket, sockaddr_in addr, int64_t session_id) 
    : SessionBase(socket, addr, session_id) {}

    void send(std::unique_ptr<PacketBase> packet) {
        packet->send(_socket, &_addr);
        _retransmit_cnt = MAX_RETRANSMITS;
        _last_msg = std::move(packet);
    }

    std::tuple<IO::PacketReader<connection>, packet_type_t> get_next() {
        while (true) {
            try {
            auto begin = std::chrono::steady_clock::now();

            // while (true) {
            //     sockaddr_in addr;
            //     IO::PacketReader<IO::Socket::UDP> reader(_socket, &_addr, true, 
            //                                              begin);
            //     auto [id, session_id] = 
            //         reader.readGeneric<packet_type_t, int64_t>();

            //     if (session_id == _session_id && 
            //         addr == _addr) {
            //         return {reader, id};
            //     } else if (id == CONN) {
            //         Packet<CONNRJT>(session_id).send(_socket, &addr);
            //     }
            // }
            return get_next_from_session<connection>(_socket, _addr, 
                                                     _session_id, begin);
            } catch (IO::timeout_error &e) {
                if (_last_msg == NULL || _retransmit_cnt <= 0)
                    throw e;

                _last_msg->send(_socket, &_addr);
                _retransmit_cnt--;
            }

        }
    }
};

template <protocol_t P>
void server_handler(Session<P> &session, Packet<CONN> conn) {
    try {
        uint64_t session_id = conn._session_id;
        uint64_t bytes_left = conn._data_len;
        std::cout << "BYTES INCOMING LEN TOTAL: " << bytes_left << "\n";

        session.send(
            std::make_unique<Packet<CONNACC>>(Packet<CONNACC>(session_id)));

        std::vector<ASIO::Packet<DATA>> data;

        while (bytes_left > 0) {
            auto [reader, packet_id] =
                session.get_next();
            if (packet_id == DATA) {
                auto data_packet = Packet<DATA>::read(reader, session_id);

                if (data_packet._packet_number != data.size()) {
                    session.send(
                        std::make_unique<Packet<RJT>>
                            (Packet<RJT>(session_id,
                                         data_packet._packet_number)));
                    throw std::runtime_error(
                        "Data packets not in order, expected:" +
                        std::to_string(data.size()) + ", received:" +
                        std::to_string(data.back()._packet_number));
                }

                std::cout << "PRINTG FILE-> ";
                std::fwrite(data_packet._data.data(), 1, 
                            data_packet._data.size(),stdout);
                std::cout << std::endl;

                if (bytes_left < data_packet._packet_byte_cnt) {
                    throw std::runtime_error(
                        "Received to much bytes: expected:" +
                        std::to_string(conn._data_len) + ", received:" +
                        std::to_string(conn._data_len - bytes_left));
                }
                bytes_left -= data_packet._packet_byte_cnt;
                data.push_back(data_packet);
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
    std::cout << "Sending file of size: " << file_size << "\n";

    session.send(std::make_unique<Packet<CONN>>(
        Packet<CONN>(session_id, P, file_size)));

    auto [reader, id] = session.get_next();

    if (id == CONNRJT) {
        return;
    }

    if (id != CONNACC) {
        throw unexpected_packet(CONN, id);
    }

    Packet<CONNACC>::read(reader, session_id);

    for (int i = 0; file.peek() != EOF; i++) {
        std::vector<char> buffor(IO::MAX_DATA_SIZE);
        file.read(buffor.data(), IO::MAX_DATA_SIZE);
        Packet<DATA> data(session_id, i, file.gcount(), buffor.data());
        session.send(std::make_unique<Packet<DATA>>(data));
    }

    auto [reader_2, id_2] = session.get_next();

    if (id_2 != RCVD) {
        throw unexpected_packet(RCVD, id_2);
    }

    Packet<RCVD>::read(reader_2, session_id);
}
}


#endif /* SERVER_HPP */