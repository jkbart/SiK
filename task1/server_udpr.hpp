#include "common.hpp"
#include "io.hpp"

#include <iostream>
#include <vector>

namespace SERVER {
using namespace ASIO;

void run_server_udpr(uint16_t port) {
    IO::Socket socket(IO::Socket::UDP);
    socket.bind(port);

    while (true) {
        try {
            sockaddr_in client_address;

            IO::PacketReader<IO::Socket::UDP> reader(socket, &client_address,
                                                     false);
            auto [id] = reader.readGeneric<packet_type_t>();
            if (id != ASIO::CONN) {
                throw unexpected_packet(CONN, id);
            }

            Packet<CONN> conn = Packet<CONN>::read(reader);

            if (conn._protocol != udpr) {
                throw std::runtime_error(
                    std::string("Unexptected protocol: expected:udpr") +
                        ", received:" + std::to_string(conn._protocol));
            }

            int64_t session_id = conn._session_id;
            int64_t bytes_left = conn._data_len;

            std::unique_ptr<PacketBase> to_retransmit =
                std::make_unique<Packet<CONNACC>>(session_id);
            int retransmit_cnt = MAX_RETRANSMITS;

            to_retransmit->send(socket, &client_address);
            retransmit_cnt--;

            std::vector<ASIO::Packet<DATA>> data;

            while (bytes_left > 0) {

                try {
                    auto [reader, packet_id] =
                        get_next_from_session<IO::Socket::UDP>(
                            socket, client_address, session_id);
                    if (packet_id == DATA) {
                        auto data_packet =
                            Packet<DATA>::read(reader, session_id);
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

                        to_retransmit.reset();
                        to_retransmit = std::make_unique<Packet<ACC>>(
                            session_id, data_packet._packet_number);
                        retransmit_cnt = MAX_RETRANSMITS;

                        to_retransmit->send(socket, &client_address);
                        retransmit_cnt--;
                    } else {
                        throw unexpected_packet(DATA, id);
                    }
                } catch (IO::timeout_error &e) {
                    if (retransmit_cnt == 0)
                        throw e;

                    to_retransmit->send(socket, &client_address);
                    retransmit_cnt--;
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