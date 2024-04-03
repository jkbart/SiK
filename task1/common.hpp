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
#include <cerrno>
#include <cstring>

#include "protconst.h"

namespace ASIO {
    constexpr int MAX_UDP_PACKET_SIZE = 65'535;
    constexpr int MAX_DATA_SIZE = 64'000;

    // enum domain_t : int {
    //     TCP,
    //     UDP
    // };

    enum sockopt_t : int {
        DEBUG = SO_DEBUG,
        BROADCAST = SO_BROADCAST,
        REUSEADDR = SO_REUSEADDR,
        KEEPALIVE = SO_KEEPALIVE,
        LINGER = SO_LINGER,
        OOBINLINE = SO_OOBINLINE,
        SNDBUF = SO_SNDBUF,
        RCVBUF = SO_RCVBUF,
        DONTROUTE = SO_DONTROUTE,
        RCVLOWAT = SO_RCVLOWAT,
        RCVTIMEO = SO_RCVTIMEO,
        SNDLOWAT = SO_SNDLOWAT,
        SNDTIMEO = SO_SNDTIMEO
    };

    class Socket {
    public:
        enum connection_t : int {
            TCP = SOCK_STREAM,
            UDP = SOCK_DGRAM
        };

        enum sockopt_t : int {
            DEBUG = SO_DEBUG,
            BROADCAST = SO_BROADCAST,
            REUSEADDR = SO_REUSEADDR,
            KEEPALIVE = SO_KEEPALIVE,
            LINGER = SO_LINGER,
            OOBINLINE = SO_OOBINLINE,
            SNDBUF = SO_SNDBUF,
            RCVBUF = SO_RCVBUF,
            DONTROUTE = SO_DONTROUTE,
            RCVLOWAT = SO_RCVLOWAT,
            RCVTIMEO = SO_RCVTIMEO,
            SNDLOWAT = SO_SNDLOWAT,
            SNDTIMEO = SO_SNDTIMEO
        };

    private:
        std::shared_ptr<int> _socket_fd{new int(-1), [](int* x) {
            if (*x == -1)
                return;

            /* Stop both reception and transmission. */
            shutdown(*x, 2);
            close(*x);
            delete x;
        }};
        connection_t _type;

    public:
        // Variable order diffrent than in socket function!!
        Socket(connection_t type, int domain = AF_INET, int protocol = 0) : _type{type} {
            *_socket_fd = socket(domain, type, protocol);
            if (*_socket_fd == -1) {
                throw std::runtime_error(std::string("Couldn't create socket: ") + std::strerror(errno));
            }
        }

        int get_fd() const {
            return *_socket_fd;
        }
        connection_t get_type() const {
            return _type;
        }

        void setsockopt(sockopt_t opt, const void *option_value, size_t opt_len) {
            int ret = ::setsockopt(*_socket_fd, SOL_SOCKET, opt, option_value, opt_len);
            if (ret == -1) {
                throw std::runtime_error(std::string("Couldn't set socket option: ") + std::strerror(errno));
            }
        }

        operator int() const {
            return *_socket_fd;
        }
    };

    template<Socket::connection_t C>
    class Reader;

    // Add timeout.
    template<>
    class Reader<Socket::TCP> {
    private:
        int _fd;
        socklen_t _address_length; 
    public:
        Reader(Socket &socket) : _fd{socket}, _address_length{(socklen_t) sizeof(sockaddr)} {}

        void readn(sockaddr *addr, void *buff, size_t n) {
            int ret = recvfrom(_fd, buff, n, MSG_WAITALL, addr, &_address_length);
            
            if (ret != n) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }
        }

        std::vector<int8_t> readn(sockaddr *addr, size_t n) {
            std::vector<int8_t> buff(n);
            int ret = recvfrom(_fd, buff.data(), n, MSG_WAITALL, addr, &_address_length);

            if (ret != n) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }

            return buff;
        }

        // Make ready for new packet.
        void reset() {
            // Reset timeout.
        }
    };

    template<>
    class Reader<Socket::UDP> {
    private:
        int _fd;
        socklen_t _address_length;
        std::vector<int8_t> _buff;
        sockaddr _addr;
        int _bytes_readed;
        bool _readed;

        void init() {
            int ret = recvfrom(_fd, _buff.data(), MAX_UDP_PACKET_SIZE, MSG_WAITALL, &_addr, &_address_length);
            if (ret != -1) {
                _buff.resize(ret);
                _readed = true;
            } else {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }
        }

    public:
        Reader(Socket &socket) : _fd{socket}, _address_length{(socklen_t) sizeof(sockaddr)}, _buff(MAX_UDP_PACKET_SIZE), _bytes_readed{0}, _readed{false} {}

        void readn(sockaddr *addr, void *buff, size_t n) {
            if (!_readed) {
                init();
            }

            if (n < _buff.size() - _bytes_readed) {
                std::memcpy(buff, _buff.data() + _bytes_readed, n);
                _bytes_readed += n;
            } else {
                throw std::runtime_error(std::string("Readed packet smaller than expected"));
            }
        }

        std::vector<int8_t> readn(sockaddr *addr, size_t n) {
            if (!_readed) {
                init();
            }

            if (n < _buff.size() - _bytes_readed) {
                _bytes_readed += n;
                return std::vector<int8_t>(_buff.begin() + _bytes_readed - n, _buff.begin() + _bytes_readed);
            } else {
                throw std::runtime_error(std::string("Readed packet smaller than expected"));
            }
        }

        // Make ready for new packet.
        void reset() {
            // Reset bufor.
        }
    };

    enum packet_type_t : int8_t {
        CONN = 1,
        CONNACC = 2,
        CONRJT = 3,
        DATA = 4,
        ACC = 5,
        RJT = 6,
        RCVD = 7
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

    class Packet {
        packet_type_t packet_id;
        virtual void send(Socket &socket, sockaddr *receiver) = 0;
        virtual void read(Socket &socket, sockaddr *sender) = 0;
    };

    class Packet_Conn : Packet {

    };
}

#endif /* COMMON_HPP */