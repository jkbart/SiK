#include "common.hpp"
#include "io.hpp"

#include <iostream>
#include <vector>

namespace CLIENT {
using namespace ASIO;

void run_client_tcp(sockaddr_in server_address) {

    IO::Socket socket(IO::Socket::TCP);
    std::cout << "Connecting... \n";

    if (connect((int)socket, (sockaddr *) &server_address,
                (socklen_t) sizeof(server_address)) < 0) {
        throw std::runtime_error("cannot connect to the server");
    }

    int64_t session_id = session_id_generate();
    std::cout << "Session: " << session_id << "\n";

    Packet<CONN>(session_id, tcp, 10).send(socket, &server_address);

    auto [reader, id] = get_next_from_session<IO::Socket::TCP>(socket, 
                                                    server_address, session_id);

    std::cout << "Readed first packet\n";

    if (id != CONNACC) {
        throw unexpected_packet(CONN, id);
    }

    Packet<CONNACC>::read(reader, session_id);

    std::cout << "Readed CONN\n";

    Packet<DATA>(session_id, 0, 10, std::vector<int8_t>(10, 42))
                                                 .send(socket, &server_address);

    auto [reader_2, id_2] = get_next_from_session<IO::Socket::TCP>(socket, 
                                                    server_address, session_id);

    if (id_2 != RCVD) {
        throw unexpected_packet(RCVD, id);
    }
    Packet<RCVD>::read(reader_2, session_id);
}
} // namespace SERVER