#include "common.hpp"
#include "io.hpp"
#include "interface.hpp"
#include "debug.hpp"

#include <iostream>
#include <string>

using namespace PPCB;
using namespace DEBUG_NS;

template <protocol_t P>
void server_handler(Session<P> &session, Packet<CONN> conn) {
    session_t session_id = conn._session_id;
    b_cnt_t bytes_left = conn._data_len;

    session.send(
        std::make_unique<Packet<CONNACC>>(session_id));

    p_cnt_t packet_number = 0;

    try {
    while (bytes_left > 0) {
        auto [reader, packet_id] =
            session.template get_next<CONN, DATA>(0, packet_number);

        if (packet_id == DATA) {
            Packet<DATA> data_packet(*reader);

            if (data_packet._packet_number != packet_number) {
                session.send(std::make_unique<Packet<RJT>>
                        (session_id, data_packet._packet_number));
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
    } catch (data_packet_smaller_than_expected &e) {
        session.send(std::make_unique<Packet<RJT>>(session_id, e._nr));
    }

    session.send(std::make_unique<Packet<RCVD>>(session_id));
}

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

            socklen_t address_length = sizeof(client_address);
            IO::Socket client_socket(accept(
                (int)socket, (sockaddr *)&client_address, &address_length));

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
                if (id == DATA) {
                    Packet<DATA> data(reader);
                    Packet<RJT>(data._session_id, data._packet_number)
                        .getSender(socket, &client_address)
                        .send<IO::Socket::UDP>();
                        
                    DBG_printer("Wating server rejected packet data nr:", 
                        data._packet_number);
                }
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