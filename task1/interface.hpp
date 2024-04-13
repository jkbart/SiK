#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include "io.hpp"
#include "common.hpp"

#include <optional>
#include <type_traits>
#include <utility>

namespace ASIO {
using namespace ASIO;

class SessionBase {
protected:
    IO::Socket &_socket;
    sockaddr_in &_addr;
    uint64_t _session_id;

public:
    uint64_t _byte_cnt;

    SessionBase(IO::Socket &socket, sockaddr_in &addr, uint64_t session_id, uint64_t byte_cnt)
    : _socket(socket), _addr(addr), _session_id(session_id), _byte_cnt(byte_cnt) {}

    void send(PacketBase &packet) {
        packet.send(_socket, &_addr);
    }

    virtual std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
    get_next(std::chrono::steady_clock::time_point to_begin = 
                std::chrono::steady_clock::now()) = 0;
};

template<IO::Socket::connection_t conn>
class Session;

template<>
class Session<IO::Socket::UDP> : SessionBase {
public:
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
    get_next(std::chrono::steady_clock::time_point to_begin = 
            std::chrono::steady_clock::now()) {
        while (true) {
            sockaddr_in addr;
            auto reader = 
                std::make_unique<IO::PacketReader<IO::Socket::UDP>>
                    (_socket, &addr, true, to_begin);
            auto [id, session_id] = 
                reader->readGeneric<packet_type_t, uint64_t>();

            if (session_id == _session_id && addr == _addr) {
                return {std::move(reader), id};
            } else if (id == CONN) {
                Packet<CONNRJT>(session_id).send(_socket, &addr);
            } else if (id == DATA) {
                auto data_packet = Packet<DATA>::read(*reader, session_id);
                Packet<RJT>(session_id, data_packet._packet_number)
                    .send(_socket, &addr);
            }
        }
    }
};

template<>
class Session<IO::Socket::TCP> : SessionBase {
public:
    std::tuple<std::unique_ptr<IO::PacketReaderBase>, packet_type_t>
    get_next(std::chrono::steady_clock::time_point to_begin = 
                std::chrono::steady_clock::now()) {
        auto reader =
            std::make_unique<IO::PacketReader<IO::Socket::TCP>>
                (_socket, NULL, true, to_begin);
        auto [id, session_id] = reader->readGeneric<packet_type_t, uint64_t>();

        if (session_id != _session_id) {
            throw std::runtime_error(
                std::string("Received unexptected session_id: ") +
                std::string("expected: ") + std::to_string(_session_id) +
                std::string("received: ") + std::to_string(session_id));
        }

        return {std::move(reader), id};
    }
};

void serve_client_retransmission(SessionBase &session) {
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
}

}

#endif /* INTERFACE_HPP */