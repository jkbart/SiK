#ifndef IO_HPP
#define IO_HPP

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <algorithm>

#include "debug.hpp"
using namespace DEBUG_NS;

#include "protconst.h"

bool operator==(const sockaddr_in &lhs, const sockaddr_in &rhs) {
    return (lhs.sin_addr.s_addr == rhs.sin_addr.s_addr) &&
           (lhs.sin_port == rhs.sin_port);
}

namespace IO {
constexpr int MAX_UDP_PACKET_SIZE = 65'535;
// constexpr int MAX_DATA_SIZE = 64'000;
constexpr int MAX_DATA_SIZE = 6;

socklen_t address_length = (socklen_t)sizeof(sockaddr_in);

class timeout_error : public std::exception {
  private:
    int _fd;
    std::string _msg;

  public:
    timeout_error(int fd)
        : _fd(fd), _msg("Socket on descriptor " + std::to_string(_fd) + " timed out") {}
    const char *what() const throw() { return _msg.c_str(); }
};

class Socket {
  public:
    enum connection_t : int { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };

    enum sockopt_t : int {
        // DEBUG = SO_DEBUG, // weird errors occur when compiling with -DDEBUG
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
    std::shared_ptr<int> _socket_fd{new int(-1), [](int *x) {
                                        if (*x == -1)
                                            return;
                                        
                                        close(*x);
                                        delete x;
                                    }};
    connection_t _type;

  public:
    void bind(uint16_t port) {
        struct sockaddr_in server_address;
        server_address.sin_family = AF_INET; // IPv4
        server_address.sin_addr.s_addr =
            htonl(INADDR_ANY); // Listening on all interfaces.
        server_address.sin_port = htons(port);

        if (::bind(*_socket_fd, (sockaddr *)&server_address,
                   (socklen_t)sizeof server_address) < 0) {
            throw std::runtime_error(std::string("Couldn't bind socket: ") +
                                     std::strerror(errno));
        }
    }

    int get_fd() const { return *_socket_fd; }

    void setsockopt(sockopt_t opt, const void *option_value, socklen_t opt_len) {
        int ret =
            ::setsockopt(*_socket_fd, SOL_SOCKET, opt, option_value, opt_len);
        if (ret == -1) {
            throw std::runtime_error(
                std::string("Couldn't set socket option: ") +
                std::strerror(errno));
        }
    }

    void setRecvTimeout(int millis) {
        timeval timeout;
        timeout.tv_sec = millis / 1000;
        timeout.tv_usec = (millis % 1000) * 1000;
        // std::cout << millis / 1000 << " " << (millis % 10000) * 1000 << "\n";
        setsockopt(RCVTIMEO, &timeout, sizeof(timeval));
    }

    void setSendTimeout(int millis) {
        timeval timeout;
        timeout.tv_sec = millis / 1000;
        timeout.tv_usec = (millis % 1000) * 1000;
        // std::cout << millis / 1000 << " " << (millis % 10000) * 1000 << "\n";
        setsockopt(SNDTIMEO, &timeout, sizeof(timeval));
    }

    void resetRecvTimeout() {
        // timeval timeout;
        // timeout.tv_sec = 0;
        // timeout.tv_usec = 0;
        // setsockopt(RCVTIMEO, &timeout, sizeof(timeval));
        setRecvTimeout(0);
    }

    void resetSendTimeout() {
        // timeval timeout;
        // timeout.tv_sec = 0;
        // timeout.tv_usec = 0;
        // setsockopt(RCVTIMEO, &timeout, sizeof(timeval));
        setSendTimeout(0);
    }

    operator int() const { return *_socket_fd; }

    Socket() = delete;
    // Variable order diffrent than in socket function!!
    Socket(connection_t type, int domain = AF_INET, int protocol = 0) {
        *_socket_fd = socket(domain, type, protocol);
        if (*_socket_fd < 0) {
            throw std::runtime_error(std::string("Couldn't create socket: ") +
                                     std::strerror(errno));
        }
        setRecvTimeout(MAX_WAIT * 1000);
        setSendTimeout(MAX_WAIT * 1000);
    }

    Socket(int fd) { 
        *_socket_fd = fd;
        setRecvTimeout(MAX_WAIT * 1000);
        setSendTimeout(MAX_WAIT * 1000);
    }
};

template<class Arg>
Arg read_single_var(char *buffor) {
    Arg var;
    std::memcpy(&var, buffor, sizeof(Arg));
    return var;
}

template<class Arg1, class Arg2>
Arg1& increment(Arg1& arg1, Arg2 arg2) {
    arg1 += arg2;
    return arg1;
}

/* Classes used to read individual packets with timout set to MAX_WAIT */
class PacketReaderBase {
  public:
    virtual void readn(void *buff, ssize_t n) = 0;
    virtual PacketReaderBase& mtb() = 0; // move ptr to begining of buffored data.
    
    std::vector<char> readn(ssize_t n) {
        std::vector<char> buffor(n);
        readn(buffor.data(), n);
        return buffor;
    }

    template <class... Args> std::tuple<Args...> readGeneric() {
        ssize_t len = (sizeof(Args) + ... + 0);
        auto buffor = readn(len);
        // int offset = 0;
        // return {*((Args *)(buffor.data() + (offset += sizeof(Args)) -
        //                    sizeof(Args)))...};
        // char *ptr = buffor.data();
        // ((printer(read_single_var<Args>((ptr += sizeof(Args)) - 
        //                                    sizeof(Args))),...));
        char *offset = buffor.data();
        // return {read_single_var<Args>((offset += sizeof(Args)) - 
        //                                sizeof(Args))...};
        return {read_single_var<Args>(increment(offset, sizeof(Args)) - 
                                      sizeof(Args))...};

    }
};

template <Socket::connection_t C> class PacketReader;

template <> class PacketReader<Socket::TCP> : public PacketReaderBase {
  private:
    Socket &_socket;
    std::chrono::steady_clock::time_point _timeout_begin;
    std::vector<char> _buff;
    ssize_t _next_byte;
    bool _needs_timeout;

  public:
    PacketReader(
        Socket &socket, sockaddr_in *, bool needs_timeout = true,
        std::chrono::steady_clock::time_point timeout_begin =
            std::chrono::steady_clock::now())
        : _socket{socket}, _buff(0), _next_byte(0),
          _timeout_begin(timeout_begin), _needs_timeout{needs_timeout} {}

    void readn(void *buff, ssize_t n) {
        if (_needs_timeout) {
            int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - _timeout_begin)
                              .count();
            if (MAX_WAIT * 1000 - timeout <= 0) {
                throw timeout_error((int)_socket);
            }

            _socket.setRecvTimeout(MAX_WAIT * 1000 - timeout);
        } else {
            _socket.resetRecvTimeout();
        }

        ssize_t old_buff_size = _buff.size();
        // ssize_t to_read = (old_buff_size + n - _next_byte);
        ssize_t to_read = std::max((n + _next_byte - old_buff_size), (ssize_t)0);
        if (0 < to_read) {
            _buff.resize(old_buff_size + to_read);

            DBG_printer("TCP start read", old_buff_size, to_read);
            ssize_t ret =
                recvfrom(_socket, &_buff[old_buff_size], to_read, 
                         MSG_WAITALL, NULL, &address_length);
            DBG_printer("TCP end read");

            if (_needs_timeout) {
                _socket.resetRecvTimeout();
                if (ret != to_read && (errno == ETIMEDOUT || errno == EAGAIN)) {
                    throw timeout_error((int)_socket);
                }
            }

            if (ret != to_read) {
                _buff.resize(old_buff_size + ret);
                throw std::runtime_error(std::string("Failed to read packet: ") +
                                         std::strerror(errno));
            }
        }

        std::memcpy(buff, &_buff[_next_byte], n);
        _next_byte += n;
    }

    PacketReaderBase& mtb() {
        _next_byte = 0;
        return *this;
    }
};

template <> class PacketReader<Socket::UDP> : public PacketReaderBase {
  private:
    Socket &_socket;
    std::vector<char> _buff;
    ssize_t _bytes_readed{0};

  public:
    PacketReader(
        Socket &socket, sockaddr_in *addr, bool needs_timeout = true,
        std::chrono::steady_clock::time_point timeout_begin =
            std::chrono::steady_clock::now())
        : _socket{socket}, _buff(MAX_UDP_PACKET_SIZE) {
        if (needs_timeout) {
            int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - timeout_begin)
                              .count();
            DBG_printer("Reader timout left", MAX_WAIT * 1000 - timeout);
            if (MAX_WAIT * 1000 - timeout <= 0) {
                throw timeout_error((int)_socket);
            }

            _socket.setRecvTimeout(MAX_WAIT * 1000 - timeout);
        } else {
            _socket.resetRecvTimeout();
        }

        ssize_t ret = recvfrom(_socket, _buff.data(), MAX_UDP_PACKET_SIZE,
                           MSG_WAITALL, (sockaddr *)addr, &address_length);

        // DBG_printer("UDP reader readed: ", std::to_string(ret), " at once");

        if (needs_timeout) {
            _socket.resetRecvTimeout();
            if (ret == -1 && (errno == ETIMEDOUT || errno == EAGAIN)) {
                DBG_printer(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - timeout_begin)
                              .count());
                DBG_printer("throiwing timeout", std::strerror(errno));
                throw timeout_error((int)_socket);
            }
        }

        if (ret == -1) {
            throw std::runtime_error(std::string("Failed to read packet: ") +
                                     std::strerror(errno));
        }

        _buff.resize(ret);
    }

    void readn(void *buff, ssize_t n) {
        DBG_printer("wanted: ", n, "ptr: ", _bytes_readed, "total: ", _buff.size());
        if (n <= (ssize_t)_buff.size() - _bytes_readed) {
            std::memcpy(buff, _buff.data() + _bytes_readed, n);
            _bytes_readed += n;
        } else {
            throw std::runtime_error(
                std::string("Readed packet smaller than expected"));
        }
    }

    PacketReaderBase& mtb() {
        _bytes_readed = 0;
        return *this;
    }
};

void send_n(Socket &socket, sockaddr_in *addr, char *buffor, ssize_t len) {
    ssize_t sent = 0;
    while (sent != len) {
        ssize_t ret = sendto((int)socket, buffor + sent, len - sent, 0,
                         (sockaddr *)addr, address_length);

        // DBG_printer("send_n sended: ", std::to_string(ret));

        if (ret == 0) {
            throw std::runtime_error(std::string("Failed to send packet: ") +
                                     std::strerror(errno));
        }
        sent += ret;
    }
}

template <class... Args>
void send_v(Socket &socket, sockaddr_in *addr, Args... args) {
    ssize_t len = (sizeof(Args) + ... + 0);
    std::vector<char> buffor(len);
    ssize_t offset = 0;
    ((std::memcpy((buffor.data() + (offset += sizeof(Args)) - sizeof(Args)),
          &args, sizeof(Args))),
     ...);

    // std::cout << "BUFFOR OT SEND: ";    
    // for(int i = 0; i < len; i++) {
    //     std::cout << (unsigned int)buffor[i] << " ";
    // } std::cout << "\n";
    // char *ptr = buffor.data();
    // ((printer(read_single_var<Args>((ptr += sizeof(Args)) - 
    //                                    sizeof(Args))),...));
    // std::cout << "\n";
    send_n(socket, addr, buffor.data(), len);
}

class PacketSender {
  private:
    Socket &_socket;
    std::vector<char> _buffor;
    sockaddr_in *_addr;

  public:
    PacketSender(Socket &socket, sockaddr_in *addr)
        : _socket(socket), _buffor(0), _addr(addr) {}

    PacketSender &add_data(void *data, size_t len) {
        ssize_t offset = _buffor.size();
        _buffor.resize(offset + len);
        std::memcpy(_buffor.data() + offset, data, len);

        return *this;
    }

    template <class... Args> PacketSender &add_var(Args... args) {
        ssize_t len = (sizeof(Args) + ... + 0);
        ssize_t offset_old = _buffor.size();

        _buffor.resize(offset_old + len);
        ssize_t offset = 0;
        ((std::memcpy(_buffor.data() + offset_old + (offset += sizeof(Args)) -
                    sizeof(Args), &args, sizeof(Args))),
         ...);

        return *this;
    }

    void send() { send_n(_socket, _addr, _buffor.data(), _buffor.size()); }
};

uint16_t read_port(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long port = strtoul(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || port > UINT16_MAX) {
        throw std::runtime_error(std::string(string) + 
                                 std::string(" is not a port valid number"));
    }
    return (uint16_t) port;
}

size_t read_size(char const *string) {
    char *endptr;
    errno = 0;
    unsigned long long number = strtoull(string, &endptr, 10);
    if (errno != 0 || *endptr != 0 || number > SIZE_MAX) {
        throw std::runtime_error(std::string(string) + 
                                 std::string(" is not a valid number"));
    }
    return number;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        throw std::runtime_error(std::string("getaddrinfo: ") + 
                                 std::string(gai_strerror(errcode)));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
            ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}
} // namespace IO

#endif /* IO_HPP */