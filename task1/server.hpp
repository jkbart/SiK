#include "common.hpp"
#include "io.hpp"

namespace SERVER {
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
            sockaddr client_address;
            sockaddr temp_address;

            IO::Socket client_socket(accept((int)socket, (struct sockaddr *) &client_address, &IO::address_length));
            if ((int)client_socket < 0) {
                throw std::runtime_error(std::string("Couldn't accept on socket: ") + std::strerror(errno));
            }

            IO::PacketReader_TCP reader(socket);
            auto [id] = reader.readGeneric<ASIO::packet_type_t>(&temp_address);
            if (id != ASIO::CONN) {
                throw std::runtime_error(std::string("Unexptected packet, expected:CONN, received:") + ASIO::packet_to_string(id));
            }

            ASIO::Packet_CONN conn = ASIO::Packet_CONN::read(reader, &temp_address);
            ASIO::Packet_CONNACC(2).send(client_socket, &client_address);

        }
    }
}