#include "common.hpp"
#include "io.hpp"
#include "handlers2.hpp"
#include "debug.hpp"

#include <iostream>
#include <string>

using namespace ASIO;
using namespace DEBUG_NS;

int main(int argc, char *argv[]) {
    try {
    if (argc != 3) {
        throw std::runtime_error("Usage: <port> <protocol>");
    }

    uint16_t port = IO::read_port(argv[1]);
    std::string s_protocol(argv[2]);

    if (s_protocol != "tcp" &&
        s_protocol != "udp") {
        throw 
            std::runtime_error("Unknown protocol name: " + s_protocol);
    }

    if (s_protocol == std::string("tcp")) {

        static const int QUEUE_LENGTH = 10;
        IO::Socket socket(IO::Socket::TCP);
        socket.bind(port);

        if (listen((int)(socket), QUEUE_LENGTH) < 0) {
            throw std::runtime_error(
                std::string("Couldn't listen on socket: ") +
                std::strerror(errno));
        }


        while (true) {
            try {
            sockaddr_in client_address;

            IO::Socket client_socket(accept(
                (int)socket, (sockaddr *)&client_address, &IO::address_length));

            DBG_printer("connected via tcp protocol");

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
            } catch (std::exception &e) {
                std::cerr << "[ERROR][SINGLE CONNECTION] " << e.what() << "\n";
            }
        }
    } else {
        IO::Socket socket(IO::Socket::UDP);
        socket.bind(port);

        while (true) {
            try {
            sockaddr_in client_address;

            socket.resetRecvTimeout();
            IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address,
                                                     false);
            socket.setRecvTimeout(MAX_WAIT * 1000);

            auto [id] = reader.readGeneric<packet_type_t>();

            DBG_printer("UDP server waiting for client received id: ", packet_to_string(id));

            if (id != ASIO::CONN) {
                continue;
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            if (conn._protocol == udp) {
                DBG_printer("connected via udp protocol");
                Session<udp> session(
                    socket, client_address, conn._session_id);

                server_handler(session, conn);
            } else if (conn._protocol == udpr) {
                DBG_printer("connected via udpr protocol");
                Session<udpr> session(
                    socket, client_address, conn._session_id);

                server_handler(session, conn);
            } else {
                throw std::runtime_error(
                    "Unknown protocol: " + std::to_string(conn._protocol));
            }
            } catch (std::exception &e) {
                std::cerr << "[ERROR][SINGLE CONNECTION] " << e.what() << "\n";
            }
        }
    } 
    // else if (s_protocol == std::string("udpr")) {
    //     std::cout << "runnning udpr protocol\n";
    //     // return 0;
    //     IO::Socket socket(IO::Socket::UDP);

    //     while (true) {
    //         sockaddr_in client_address;

    //         IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address,
    //                                                  false);
    //         auto [id] = reader.readGeneric<packet_type_t>();
    //         if (id != ASIO::CONN) {
    //             continue;
    //             // throw unexpected_packet(CONN, id);
    //         }

    //         Packet<CONN> conn = Packet<CONN>::read(reader);

    //         Session<udpr> session(
    //             socket, client_address, conn._session_id);

    //         server_handler(session, conn);
    //     }
    // }
    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
    }
}