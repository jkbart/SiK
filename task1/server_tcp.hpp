#include "common.hpp"
#include "io.hpp"

#include <iostream>
#include <vector>

namespace SERVER {
using namespace ASIO;



void run_server_tcp(uint16_t port) {
    static const int QUEUE_LENGTH = 10;
    IO::Socket socket(IO::Socket::TCP);
    socket.bind(port);

    if (listen((int)(socket), QUEUE_LENGTH) < 0) {
        throw std::runtime_error(std::string("Couldn't listen on socket: ") +
                                 std::strerror(errno));
    }

    while (true) {
        std::cout << "wainting for CONN\n";
        try {
            sockaddr_in client_address;

            IO::Socket client_socket(accept(
                (int)socket, (sockaddr *)&client_address, &IO::address_length));

            if ((int)client_socket < 0) {
                throw std::runtime_error(
                    std::string("Couldn't accept on socket: ") +
                    std::strerror(errno));
            }

            IO::PacketReader<IO::Socket::TCP> reader(client_socket, NULL);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            if (conn._protocol != tcp) {
                throw std::runtime_error(
                    std::string("Unexptected protocol: expected:tcp") +
                        ", received:" + std::to_string(conn._protocol));
            }

            int64_t session_id = conn._session_id;
            int64_t bytes_left = conn._data_len;

            ASIO::Packet<CONNACC>(session_id)
                .send(client_socket, &client_address);

            std::vector<ASIO::Packet<DATA>> data;

            while (bytes_left > 0) {
                auto [reader, packet_id] =
                    get_next_from_session<IO::Socket::TCP>(
                        client_socket, client_address, session_id);
                if (packet_id == DATA) {
                    auto data_packet = Packet<DATA>::read(reader, session_id);
                    if (data_packet._packet_number != data.size()) {
                        Packet<RJT>(session_id, data_packet._packet_number)
                            .send(socket, &client_address);
                        throw std::runtime_error(
                            "Data packets not in order, expected:" +
                            std::to_string(data.size()) + ", received:" +
                            std::to_string(data.back()._packet_number));
                    }
                    bytes_left -= data_packet._packet_byte_cnt;
                    data.push_back(data_packet);
                } else {
                    throw unexpected_packet(DATA, id);
                }
            }
            if (bytes_left < 0) {
                throw std::runtime_error(
                    "Received to much bytes: expected:" +
                    std::to_string(conn._data_len) + ", received:" +
                    std::to_string(conn._data_len - bytes_left));
            }

            Packet<RCVD>(session_id).send(client_socket, &client_address);

        } catch (std::exception &e) {
            std::cerr << "[ERROR] " << e.what();
        }
    }
}
} // namespace SERVER