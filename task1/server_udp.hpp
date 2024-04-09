#include "common.hpp"
#include "io.hpp"

#include <iostream>
#include <vector>

namespace SERVER {
using namespace ASIO;

void run_server_udp(uint16_t port) {
    IO::Socket socket(IO::Socket::UDP);
    socket.bind(port);

    while (true) {
        try {
            sockaddr_in client_address;

            IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            if (conn._protocol != udp) {
                throw std::runtime_error(
                    std::string("Unexptected protocol: expected:udp") +
                        ", received:" + std::to_string(conn._protocol));
            }

            int64_t session_id = conn._session_id;
            int64_t bytes_left = conn._data_len;

            ASIO::Packet<CONNACC>(session_id).send(socket, &client_address);

            std::vector<ASIO::Packet<DATA>> data;

            while (bytes_left > 0) {
                auto [reader, packet_id] =
                    get_next_from_session<IO::Socket::UDP>(
                        socket, client_address, session_id);
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

            Packet<RCVD>(session_id).send(socket, &client_address);

        } catch (std::exception &e) {
            std::cerr << "[ERROR] " << e.what();
        }
    }
}
} // namespace SERVER