#include "common.hpp"
#include "io.hpp"
#include "interface.hpp"
#include "debug.hpp"

#include <iostream>
#include <string>

using namespace PPCB;
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

            socket.resetRecvTimeout();
            IO::Socket client_socket(accept(
                (int)socket, (sockaddr *)&client_address, &IO::address_length));

            DBG_printer("connected via tcp protocol");

            IO::PacketReader<IO::Socket::TCP> reader(client_socket, NULL);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != CONN) {
                throw unexpected_packet(CONN, std::nullopt, id, std::nullopt);
            }

            reader.mtb();
            Packet<CONN> conn(reader);

            Session<tcp> session(
                client_socket, client_address, conn._session_id);

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

            DBG_printer("UDP server waiting for client received id: ", 
                packet_to_string(id));

            if (id != CONN) {
                continue;
            }

            reader.mtb();
            Packet<CONN> conn(reader);

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
    } catch (std::exception &e) {
        std::cerr << "[ERROR][FATAL] " << e.what() << "\n";
    }
}