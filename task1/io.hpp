#ifndef IO_HPP
#define IO_HPP

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
#include <chrono>

#include "protconst.h"

namespace IO {
    constexpr int MAX_UDP_PACKET_SIZE = 65'535;
    constexpr int MAX_DATA_SIZE = 64'000;

    socklen_t address_length = (socklen_t) sizeof(sockaddr);

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
        Socket(connection_t type, int domain = AF_INET, int protocol = 0) {
            *_socket_fd = socket(domain, type, protocol);
            if (*_socket_fd == -1) {
                throw std::runtime_error(std::string("Couldn't create socket: ") + std::strerror(errno));
            }
        }

        Socket(int fd) {
            *_socket_fd = fd;
        }

        void bind(uint16_t port) {
            struct sockaddr_in server_address;
            server_address.sin_family = AF_INET; // IPv4
            server_address.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
            server_address.sin_port = htons(port);

            if (::bind(*_socket_fd, (sockaddr *) &server_address, (socklen_t) sizeof server_address) < 0) {
                throw std::runtime_error(std::string("Couldn't bind socket: ") + std::strerror(errno));
            }
        }

        int get_fd() const {
            return *_socket_fd;
        }

        void setsockopt(sockopt_t opt, const void *option_value, size_t opt_len) {
            int ret = ::setsockopt(*_socket_fd, SOL_SOCKET, opt, option_value, opt_len);
            if (ret == -1) {
                throw std::runtime_error(std::string("Couldn't set socket option: ") + std::strerror(errno));
            }
        }

        void setRecvTimeout(int millis) {
            timeval timeout;
            timeout.tv_sec = millis / 1000;
            timeout.tv_usec = (millis % 10000) * 1000;
            setsockopt(RCVTIMEO, &timeout, sizeof(timeval));
        }

        void resetRecvTimeout() {
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
            setsockopt(RCVTIMEO, &timeout, sizeof(timeval));
        }

        operator int() const {
            return *_socket_fd;
        }
    };

    /* Classes used to read individual packets with timout set to MAX_WAIT */
    class PacketReaderBase {
    public:
        virtual void readn(sockaddr *addr, void *buff, size_t n) = 0;
        std::vector<int8_t> readn(sockaddr *addr, size_t n) {
            std::vector<int8_t> buffor(n);
            readn(addr, buffor.data(), n);
            return buffor;
        }

        template<class... Args>
        std::tuple<Args...> readGeneric(sockaddr *addr){
            int len = (sizeof(Args) + ... + 0);
            auto buffor = readn(addr, len);
            int offset = 0;
            return {*((Args*)(buffor.data() + (offset += sizeof(Args)) - sizeof(Args)))...};
        }
    };

    template<Socket::connection_t C>
    class PacketReader;

    template<>
    class PacketReader<Socket::TCP> : public PacketReaderBase {
    private:
        Socket &_socket;
        std::chrono::steady_clock::time_point _timeout_begin;
    public:
        PacketReader<Socket::TCP>(Socket &socket, std::chrono::steady_clock::time_point timeout_begin=std::chrono::steady_clock::now()) : _socket{socket}, _timeout_begin(timeout_begin) {}

        void readn(sockaddr *addr, void *buff, size_t n) {
            int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _timeout_begin).count();
            if (MAX_WAIT * 1000 - timeout <= 0) {
                throw std::runtime_error(std::string("Reading packet timed out."));
            }

            _socket.setRecvTimeout(MAX_WAIT * 1000 - timeout);

            int ret = recvfrom(_socket, buff, n, MSG_WAITALL, addr, &address_length);

            _socket.resetRecvTimeout();

            if (ret != n) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }
        }
    };

    template<>
    class PacketReader<Socket::UDP> : public PacketReaderBase{
    private:
        Socket &_socket;
        socklen_t _address_length;
        std::vector<int8_t> _buff;
        sockaddr _addr;
        int _bytes_readed;
        bool _readed;

    public:
        PacketReader<Socket::UDP>(Socket &socket, std::chrono::steady_clock::time_point timeout_begin=std::chrono::steady_clock::now()) : _socket{socket}, _address_length{(socklen_t) sizeof(sockaddr)}, _buff(MAX_UDP_PACKET_SIZE), _bytes_readed{0}, _readed{false} {
            int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeout_begin).count();
            if (MAX_WAIT * 1000 - timeout <= 0) {
                throw std::runtime_error(std::string("Reading packet timed out."));
            }

            _socket.setRecvTimeout(MAX_WAIT * 1000 - timeout);

            int ret = recvfrom(_socket, _buff.data(), MAX_UDP_PACKET_SIZE, MSG_WAITALL, &_addr, &_address_length);

            _socket.resetRecvTimeout();
            if (ret == -1) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            } 

            _buff.resize(ret);
            _readed = true;

        }

        void readn(sockaddr *addr, void *buff, size_t n) {
            if (n < _buff.size() - _bytes_readed) {
                std::memcpy(buff, _buff.data() + _bytes_readed, n);
                _bytes_readed += n;
            } else {
                throw std::runtime_error(std::string("Readed packet smaller than expected"));
            }
        }
    };


    void send_n(Socket &socket, sockaddr *addr, int8_t *buffor, int len) {
        int sent = 0;
        while (sent != len) {
            int ret = sendto((int)socket, buffor + sent, len - sent, 0, addr, address_length);
            if (ret == 0) {
                throw std::runtime_error(std::string("Failed to send packet: ") + std::strerror(errno));
            }
            sent += ret;
        }

        // ssize_t sent_length = sendto(int socketfd, send_buffer, message_length, send_flags,
        //                              (struct sockaddr *) &server_address, address_length);
    }

    template<class... Args>
    void send_v(Socket &socket, sockaddr *addr,  Args... args){
        int len = (sizeof(Args) + ... + 0);
        std::vector<int8_t> buffor(len); 
        int offset = 0;
        ((*(Args*)(buffor.data() + (offset += sizeof(Args)) - sizeof(Args)) = args),...);
        send_n(socket, addr, buffor.data(), len);
    }


    class PacketSender {
    private:
        Socket &_socket;
        std::vector<int8_t> _buffor;
        sockaddr *_addr;
    public:
        PacketSender(Socket &socket, sockaddr *addr) : _socket(socket), _buffor(0), _addr(addr) {}

        PacketSender& add_data(void *data, size_t len) {
            size_t offset = _buffor.size();
            _buffor.resize(offset + len);
            std::memcpy(_buffor.data() + offset, data, len);

            return *this;
        }

        template<class... Args>
        PacketSender& add_var(Args... args){
            size_t len = (sizeof(Args) + ... + 0);
            size_t offset_old = _buffor.size();

            _buffor.resize(offset_old + len);
            int offset = 0;
            ((*(Args*)(_buffor.data() + offset_old + (offset += sizeof(Args)) - sizeof(Args)) = args),...);

            return *this;
        }

        void send() {
            send_n(_socket, _addr, _buffor.data(), _buffor.size());
        }
    };
}

#endif /* IO_HPP */