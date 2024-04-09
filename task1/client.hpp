#include "common.hpp"

namespace CLIENT {
    class Client {
    private:
        ASIO::Socket _socket;
    public:
        Client(ASIO::Socket socket) : _socket(socket) {}
        virtual void connect(const sockaddr &address) = 0;
        virtual void send_messege(std::vector<ASIO::packet_t> messege) = 0;
        virtual ASIO::packet_t recv_messege() = 0;
        virtual void close() = 0;
    };

    // ASIO::packet_t UDP_read_packet(int sock_fd, const sockaddr &addr) {
    //     static std::vector<char> data(ASIO::MAX_DATA_SIZE);
    //     ssize_t readed_len = recv(sock_fd, &data[0], data.size(), 0, &addr, (socklen_t) sizeof(ASIO::addrlen));
        
    //     if (readed_len <= 0)
    //         throw std::runtime_error("Failed to read data from socket");
    //     return std::vector<char>(data.begin(), data.begin() + readed_len);
    // }

}