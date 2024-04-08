#include "common.hpp"
#include "io.hpp"

#include <iostream>
#include <vector>

namespace SERVER {
    using namespace ASIO;

    template<IO::Socket::connection_t C>
    std::tuple<IO::PacketReader<C>&, packet_type_t> get_next_from_session(IO::Socket &socket, sockaddr client_address);

    template<>
    std::tuple<IO::PacketReader<IO::Socket::UDP>&, packet_type_t> get_next_from_session<IO::Socket::UDP>(IO::Socket &socket, sockaddr client_address) {
        while (true) {
            sockaddr addr;
            IO::PacketReader<IO::Socket::UDP> reader(socket);
            auto [id, session_id] = 
                reader.readGeneric<packet_type_t, int64_t>(&addr);
            
        }
    }

    template<>
    std::tuple<IO::PacketReader<IO::Socket::TCP>&, packet_type_t> get_next_from_session<IO::Socket::TCP>(IO::Socket &socket, sockaddr client_address) {
        
    }


    template<ASIO::protocol_t P>
    void run_server(uint16_t port);

    template<>
    void run_server<ASIO::tcp>(uint16_t port) {
        static const int QUEUE_LENGTH = 10;
        IO::Socket socket(IO::Socket::TCP);
        socket.bind(port);

        if (listen((int)(socket), QUEUE_LENGTH) < 0) {
            throw std::runtime_error(std::string("Couldn't listen on socket: ") + std::strerror(errno));
        }

        while(true) {
            try {
            sockaddr client_address;
            sockaddr temp_address;

            IO::Socket client_socket(accept((int)socket, (struct sockaddr *) &client_address, &IO::address_length));
            if ((int)client_socket < 0) {
                throw std::runtime_error(std::string("Couldn't accept on socket: ") + std::strerror(errno));
            }

            IO::PacketReader<IO::Socket::TCP> reader(client_socket);
            auto [id] = reader.readGeneric<ASIO::packet_type_t>(&temp_address);
            if (id != ASIO::CONN) {
                throw std::runtime_error(std::string("Unexptected packet, expected:CONN, received:") + ASIO::packet_to_string(id));
            }

            Packet<CONN> conn = Packet<CONN>::read(reader, &temp_address);

            int64_t session_id = conn._session_id;
            int64_t bytes_left = conn._data_len;

            ASIO::Packet<CONNACC>(session_id).send(client_socket, &client_address);

            std::vector<ASIO::Packet<DATA>> data;

                while (bytes_left > 0) {
                    IO::PacketReader<IO::Socket::TCP> data_reader(client_socket);
                    auto data_id = data_reader.readGeneric<ASIO::packet_type_t>(&temp_address);

                    if (id != ASIO::DATA) {
                        throw std::runtime_error(std::string("Unexptected packet, expected:DATA, received:") + ASIO::packet_to_string(id));
                    }

                    data.push_back(ASIO::Packet<DATA>::read(data_reader, &temp_address));

                    if (data.back()._session_id != session_id) {
                        throw std::runtime_error(std::string("Unexptected session id, expected:" + std::to_string(session_id) + std::string(", received:") + std::to_string(data.back()._session_id)));
                    }

                    if (data.back()._packet_number != data.size() - 1) {
                        throw std::runtime_error(std::string("Data packets not in order, expected:" + std::to_string( data.size() - 1) + std::string(", received:") + std::to_string(data.back()._packet_number)));
                    }

                    bytes_left -= data.back()._packet_byte_cnt;
                }

            } catch (std::exception &e) {
                std::cerr << "[ERROR] " << e.what();
            }
        }
    }
}