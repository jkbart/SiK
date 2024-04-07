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
#include <chrono>

#include "protconst.h"

namespace ASIO {
    constexpr int MAX_UDP_PACKET_SIZE = 65'535;
    constexpr int MAX_DATA_SIZE = 64'000;


    static socklen_t address_length = (socklen_t) sizeof(sockaddr);

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

    template<Socket::connection_t C>
    class PacketReader;

    // Add timeout.
    template<>
    class PacketReader<Socket::TCP> {
    private:
        Socket _socket;
        int time_left;
    public:
        PacketReader(Socket &socket) : _socket{socket}, time_left(MAX_WAIT * 1000) {}

        void readn(sockaddr *addr, void *buff, size_t n) {
            if (time_left <= 0) {
                throw std::runtime_error(std::string("Reading packet timed out."));
            }

            _socket.setRecvTimeout(time_left);
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            int ret = recvfrom(_socket, buff, n, MSG_WAITALL, addr, &address_length);

            int duration_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
            time_left -= duration_time;
            _socket.resetRecvTimeout();

            // if (time_left <= 0) {
            //     throw std::runtime_error(std::string("Reading packet timed out."));
            // }

            if (ret != n) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }
        }

        std::vector<int8_t> readn(sockaddr *addr, size_t n) {
            if (time_left <= 0) {
                throw std::runtime_error(std::string("Reading packet timed out."));
            }

            std::vector<int8_t> buff(n);

            _socket.setRecvTimeout(time_left);
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

            int ret = recvfrom(_socket, buff.data(), n, MSG_WAITALL, addr, &address_length);

            int duration_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
            time_left -= duration_time;
            _socket.resetRecvTimeout();

            // if (time_left <= 0) {
            //     throw std::runtime_error(std::string("Reading packet timed out."));
            // }

            if (ret != n) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            }

            return buff;
        }

        // Convinient way to read variables.
        template<class... Args>
        std::tuple<Args...> readGeneric(sockaddr *addr){
            int len = (sizeof(Args) + ... + 0);
            auto buffor = readn(addr, len);
            int offset = 0;
            return {*((Args*)(buffor.data() + (offset += sizeof(Args)) - sizeof(Args)))...};
        }
    };

    template<>
    class PacketReader<Socket::UDP> {
    private:
        Socket _socket;
        socklen_t _address_length;
        std::vector<int8_t> _buff;
        sockaddr _addr;
        int _bytes_readed;
        bool _readed;

        void init() {
            _socket.setRecvTimeout(MAX_WAIT * 1000);

            int ret = recvfrom(_socket, _buff.data(), MAX_UDP_PACKET_SIZE, MSG_WAITALL, &_addr, &_address_length);

            _socket.resetRecvTimeout();
            if (ret == -1) {
                throw std::runtime_error(std::string("Failed to read packet: ") + std::strerror(errno));
            } 

            _buff.resize(ret);
            _readed = true;
        }

    public:
        PacketReader(Socket &socket) : _socket{socket}, _address_length{(socklen_t) sizeof(sockaddr)}, _buff(MAX_UDP_PACKET_SIZE), _bytes_readed{0}, _readed{false} {}

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

        // Convinient way to read variables.
        template<class... Args>
        std::tuple<Args...> readGeneric(sockaddr *addr){
            int len = (sizeof(Args) + ... + 0);
            auto buffor = readn(addr, len);
            int offset = 0;
            return {*((Args*)(buffor.data() + (len += sizeof(Args)) - sizeof(Args)))...};
        }
    };

    void send_n(Socket &socket, void *buffor, int len) {
        int sent = 0;
        while (sent != len) {
            int ret = sendto((int)socket, buffor.data() + sent, len - sent, 0, addr, s);
            if (ret == 0) {
                throw std::runtime_error(std::string("Failed to send packet: ") + std::strerror(errno));
            }
            sent += ret;
        }

        ssize_t sent_length = sendto(socket_fd, send_buffer, message_length, send_flags,
                                     (struct sockaddr *) &server_address, address_length);
    }


    template<class... Args>
    void send_v(Socket &socket, Args... args){
        int len = (sizeof(Args) + ... + 0);
        std::vector<int8_t> buffor(len); 
        int offset = 0;
        ((*(Args*)(buffor.data() + (offset += sizeof(Args)) - sizeof(Args)) = args),...);
        send_n(socket, buffor.data(), len);
    }

    enum protocol_t : int8_t {
        tcp = 1,
        udp = 2,
        udpr = 3
    };

    enum packet_type_t : int8_t {
        CONN = 1,
        CONNACC = 2,
        CONNRJT = 3,
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
        case CONNRJT:
            return "CONNRJT";
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

    // struct packet_t {
    //     sockaddr_in client_address;
    //     std::vector<char> data;
    // };


    class Packet {
    private:
        const int64_t _session_id;
    protected:
        Packet(int64_t session_id) : _session_id(session_id) {}
    public:
        virtual void send(Socket &socket, sockaddr *receiver) = 0;
        virtual packet_type_t getID() = 0;

    };

    class Packet_CONN : Packet {
    private:
        static const packet_type_t _id = CONN;
        const protocol_t _protocol;
        const int64_t _data_len;
    public:
        Packet_CONN(int session_id, protocol_t protocol, int64_t data_len) : Packet(session_id), _protocol(protocol), _data_len(data_len) {}
        void send(Socket &socket, sockaddr *receiver) {

        }

        static Packet_CONN read(PacketReader &reader, sockaddr *addr) {
            auto [session_id, protocol, data_len] = 
                reader.readGeneric<int64_t, protocol_t, int64_t>(addr);
            return Packet_CONN(session_id, protocol, data_len);
        }
    };

    class Packet_CONNACC : Packet {
    private:
        static const packet_type_t _id = CONNACC;
    };

    class Packet_CONNRJT : Packet {
    private:
        static const packet_type_t _id = CONNRJT;
    };

    class Packet_DATA : Packet {
    private:
        static const packet_type_t _id = DATA;
        const int64_t packet_number;
        const int32_t packet_byte_cnt;
        const std::vector<int8_t> data;
    };

    class Packet_ACC : Packet {
    private:
        static const packet_type_t _id = ACC;
        const int64_t packet_number;
    };

    class Packet_RJT : Packet {
    private:
        static const packet_type_t _id = RJT;
        const int64_t packet_number;
    };

    class Packet_RCVD : Packet {
    private:
        static const packet_type_t _id = RCVD;
    };
}

#endif /* COMMON_HPP */