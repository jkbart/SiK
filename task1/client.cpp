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
        std::cout << "Connecting... \n";

        if (connect((int)socket, (sockaddr *) &server_address,
                    (socklen_t) sizeof(server_address)) < 0) {
            throw std::runtime_error("cannot connect to the server");
        }

        Session<tcp> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    } else if (s_protocol == "udp") {
        IO::Socket socket(IO::Socket::UDP);
        std::cout << "Connecting... \n";

        Session<udp> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    } else if (s_protocol == "udpr") {
        IO::Socket socket(IO::Socket::UDP);
        std::cout << "Connecting... \n";

        Session<udpr> session(socket, server_address, session_id);

        client_handler(session, session_id, file);
    }
    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
    }
}