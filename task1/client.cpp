#include "common.hpp"
#include "io.hpp"
#include "handlers.hpp"

#include <iostream>
#include <string>

using namespace ASIO;

std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream file(filename, std::ifstream::ate | std::ifstream::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open the file");
        return 1;
    }
    return file.tellg(); 
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        std::cerr << "[ERROR] Usage: <protocol> <ip> <port> <file>\n";
        return 1;
    }

    std::string s_protocol(argv[1]);
    uint16_t port = IO::read_port(argv[3]);

    sockaddr_in server_address = IO::get_server_address(argv[2], port);

    std::ifstream file(argv[4], std::ios::binary); 
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open the file");
        return 1;
    }

    size_t file_size = filesize(argv[4]);

    std::optional<Packet<CONN>> conn;

    if (s_protocol == "tcp") {

        IO::Socket socket(IO::Socket::TCP);
        std::cout << "Connecting... \n";

        if (connect((int)socket, (sockaddr *) &server_address,
                    (socklen_t) sizeof(server_address)) < 0) {
            throw std::runtime_error("cannot connect to the server");
        }

        int64_t session_id = session_id_generate();
        Session<tcp> session(socket, server_address, session_id);

        client_handler(session, session_id, std::move(file), file_size);
    } else if (s_protocol == "udp") {
        IO::Socket socket(IO::Socket::UDP);
        std::cout << "Connecting... \n";

        int64_t session_id = session_id_generate();
        Session<udp> session(socket, server_address, session_id);

        client_handler(session, session_id, std::move(file), file_size);
    } else if (s_protocol == "udpr") {
        IO::Socket socket(IO::Socket::UDP);
        std::cout << "Connecting... \n";

        int64_t session_id = session_id_generate();
        Session<udpr> session(socket, server_address, session_id);

        client_handler(session, session_id, std::move(file), file_size);
    }
}