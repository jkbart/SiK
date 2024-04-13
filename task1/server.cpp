#include "common.hpp"
#include "io.hpp"
#include "handlers.hpp"

#include <iostream>
#include <string>

using namespace ASIO;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "[ERROR] Usage: <port> <protocol>\n";
        return 1;
    }

    uint16_t port = IO::read_port(argv[1]);
    std::string s_protocol(argv[2]);

    if (s_protocol == std::string("tcp")) {
        std::cout << "runnning tcp protocol\n";
        // return 0;

        static const int QUEUE_LENGTH = 10;
        IO::Socket socket(IO::Socket::TCP);
        socket.bind(port);

        if (listen((int)(socket), QUEUE_LENGTH) < 0) {
            throw std::runtime_error(std::string("Couldn't listen on socket: ") +
                                     std::strerror(errno));
        }

        while (true) {
            sockaddr_in client_address;

            IO::Socket client_socket(accept(
                (int)socket, (sockaddr *)&client_address, &IO::address_length));

            IO::PacketReader<IO::Socket::TCP> reader(client_socket, NULL);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            Session<tcp> session(
                client_socket, client_address, conn._session_id);

            std::cout << "server_handler\n";
            server_handler(session, conn);
        }
    } else if (s_protocol == std::string("udp")) {
        std::cout << "runnning udp protocol\n";
        // return 0;
        IO::Socket socket(IO::Socket::UDP);

        while (true) {
            sockaddr_in client_address;

            IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address,
                                                     false);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                continue;
                // throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            Session<udp> session(
                socket, client_address, conn._session_id);

            server_handler(session, conn);

        }
    } else if (s_protocol == std::string("udpr")) {
        std::cout << "runnning udpr protocol\n";
        // return 0;
        IO::Socket socket(IO::Socket::UDP);

        while (true) {
            sockaddr_in client_address;

            IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address,
                                                     false);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                continue;
                // throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            Session<udpr> session(
                socket, client_address, conn._session_id);

            server_handler(session, conn);
        }
    }
}