#ifndef COMMON_HPP
#define COMMON_HPP

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <stdexcept>
#include <vector>
#include <utility>
#include <string>
#include <memory>

#include "protconst.h"

namespace NET {
    constexpr int MAX_UDP_PACKET_SIZE = 65'535;
    constexpr int MAX_DATA_SIZE = 64'000;


    class Socket {
    private:
        int _socket_fd = -1;

    public:
        Socket(int domain, int type, int protocol) {
            _socket_fd = socket(domain, type, protocol);
            if (_socket_fd == -1) {
                throw std::runtime_error("Couldn't create socket");
            }
        }

        // Socket(const Socket& other) = delete;
        // Socket& operator=(const Socket& other) = delete;

        // Socket(Socket&& other) noexcept // move constructor
        // : _socket_fd(std::move(other._socket_fd)) { 
        //     other._socket_fd = -1; 
        // }

        // Socket& operator=(Socket&& other) noexcept { // move assignment
        //     std::swap(_socket_fd, other._socket_fd);
        // }

        int get_fd() {
            return _socket_fd;
        }

        ~Socket() {
            if (_socket_fd == -1)
                return;

            /* Stop both reception and transmission. */
            shutdown(_socket_fd, 2);
            close(_socket_fd);
        }
    };

    enum packet_type_t : int8_t {
        CONN,
        CONNACC,
        CONRJT,
        DATA,
        ACC,
        RJT,
        RCVD
    };

    std::string packet_to_string(packet_type_t packet_type) {
        switch(packet_type) {
        case CONN:
            return "CONN";
        case CONNACC:
            return "CONNACC";
        case CONRJT:
            return "CONRJT";
        case DATA:
            return "DATA";
        case ACC:
            return "ACC";
        case RJT:
            return "RJT";
        case RCVD:
            return "RCVD";
        default:
            return "Unkown packet type";
        }
    }

    struct packet_t {
        sockaddr_in client_address;
        std::vector<char> data;
    };

    class Server {
    private:
        Socket _socket;
    public:
        Server(Socket socket) : _socket(socket) {}
        virtual void setup(const sockaddr &address) = 0;
        virtual void new_connection() = 0;
        virtual packet_t recv_messege() = 0;
        virtual void send_messege(const sockaddr &address) = 0;
        virtual void anserw_back() = 0;
        virtual void close() = 0;
    };

    class Client {
    private:
        Socket _socket;
    public:
        Client(Socket socket) : _socket(socket) {}
        virtual void connect(const sockaddr &address) = 0;
        virtual void send_messege(std::vector<packet_t> messege) = 0;
        virtual packet_t recv_messege() = 0;
        virtual void close() = 0;
    };

    packet_t UDP_read_packet(int sock_fd, const sockaddr &addr) {
        static std::vector<char> data(MAX_DATA_SIZE);
        ssize_t readed_len = recv(sock_fd, &data[0], data.size(), 0, &addr, (socklen_t) sizeof(addrlen));
        
        if (readed_len <= 0)
            throw std::runtime_error("Failed to read data from socket");
        return std::vector<char>(data.begin(), data.begin() + readed_len);
    }

    class UDP_server : Server {
    private:
        Socket _socket;
    public:
        void setup(const sockaddr &address) {
            if (bind(_socket.get_fd(), &address, sizeof(sockaddr)) != 0) {
                throw std::runtime_error("Couldn't bind socket(" + 
                    std::to_string(_socket.get_fd()) + ")");
            }
        }

        void new_connection() {

        }
    };
}

#endif /* COMMON_HPP */