#include "common.hpp"
#include "io.hpp"
#include "interface.hpp"
#include "debug.hpp"

#include <iostream>
#include <string>

using namespace PPCB;
using namespace DEBUG_NS;


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
        Packet<RJT> rjt(*reader_2);
        throw rejected_data(rjt._packet_number);
    } else {
        throw unexpected_packet(RCVD, std::nullopt, id, std::nullopt);
    }
}

int main(int argc, char *argv[]) {
    try {
    if (argc != 5) {
        throw std::runtime_error("Usage: <protocol> <ip> <port> <file>");
    }

    std::string s_protocol(argv[1]);
    uint16_t port = IO::read_port(argv[3]);

    sockaddr_in server_address = IO::get_server_address(argv[2], port);

    session_t session_id = session_id_generate();

    File file(argv[4], session_id);

    std::optional<Packet<CONN>> conn;

    if (s_protocol == "tcp") {
        IO::Socket socket(IO::Socket::TCP);
        DBG_printer("Connecting...");

        if (connect((int)socket, (sockaddr *) &server_address,
                    (socklen_t) sizeof(server_address)) < 0) {
            throw std::runtime_error("cannot connect to the server");
        }

        Session<tcp> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    } else if (s_protocol == "udp") {
        IO::Socket socket(IO::Socket::UDP);
        DBG_printer("Connecting...");

        Session<udp> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    } else if (s_protocol == "udpr") {
        IO::Socket socket(IO::Socket::UDP);
        DBG_printer("Connecting...");

        Session<udpr> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    }
    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
    }
}