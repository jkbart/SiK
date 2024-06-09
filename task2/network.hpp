#ifndef NET_HPP
#define NET_HPP

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <endian.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>


#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <memory>
#include <cstring>
#include <tuple>
#include <functional>

#include "exceptions.hpp"

#include "debug.hpp"
using namespace DEBUG_NS;

namespace NET {

uint16_t read_port(char const *string) {
    unsigned long port = std::stoul(string);
    if (port > UINT16_MAX) {
        throw std::invalid_argument("Incorrect port format: " + 
                                    std::string(string));
    }
    return (uint16_t) port;
}


std::string getname(int socket, 
                    std::function<int(int, sockaddr*, socklen_t*)> f) {
    sockaddr_storage st;
    socklen_t len;

    std::cout << "getname " << socket << "\n";
    int sys_call_ret = f(socket, (sockaddr*) &st, &len);
    if (sys_call_ret != 0) {
        throw syscall_error("get...name", sys_call_ret);
    }
    int domain;
    uint16_t port;

    if (len == sizeof(sockaddr_in)) {
        domain = AF_INET;
        port = ntohs(((sockaddr_in*)&st)->sin_port);
    } else if (len == sizeof(sockaddr_in6)) {
        domain = AF_INET6;
        port = ntohs(((sockaddr_in6*)&st)->sin6_port);
    } else {
        throw syscall_error("get...name::len", len);
    }

    char buffor[INET6_ADDRSTRLEN];
    if (nullptr == inet_ntop(domain, &st, buffor, INET6_ADDRSTRLEN)) {
        throw syscall_error("inet_ntop", 0);
    }

    return std::string(buffor) + ":" + std::to_string(port);
}

std::string getpeername(int socket) {
    return getname(socket, ::getpeername);
}

std::string getsockname(int socket) {
    return getname(socket, ::getsockname);
}

int connect(char *host, uint16_t port, int domain) {

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = domain;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);

    if (errcode != 0) {
        throw syscall_error(std::string("getaddrinfo(") + 
                            std::string(gai_strerror(errcode)) + 
                            std::string(")"), errcode);
    }

    int sfd = socket(address_result->ai_family, 
                     address_result->ai_socktype, 
                     address_result->ai_protocol);

    if (sfd == -1) {
        freeaddrinfo(address_result);
        throw syscall_error("socket", sfd);
    }

    if (address_result->ai_family == AF_INET6) {
        debuglog << "Connecting using ipv6\n";
        struct sockaddr_in6 server_address;
        server_address.sin6_family = AF_INET6;  // IPv6
        server_address.sin6_addr =
                ((struct sockaddr_in6 *) (address_result->ai_addr))->sin6_addr;
        server_address.sin6_port = htons(port);
        int sys_call_ret = 
            ::connect(sfd, (sockaddr*)&server_address, sizeof(sockaddr_in6));
        if (sys_call_ret != 0) {
            freeaddrinfo(address_result);
            close(sfd);
            throw syscall_error("connect", sys_call_ret);
        }
    } else if (address_result->ai_family == AF_INET) {
        debuglog << "Connecting using ipv4\n";
        struct sockaddr_in server_address;
        server_address.sin_family = AF_INET;  // IPv4
        server_address.sin_addr =
                ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr;
        server_address.sin_port = htons(port);
        int sys_call_ret = 
            ::connect(sfd, (sockaddr*)&server_address, sizeof(sockaddr_in));
        if (sys_call_ret != 0) {
            freeaddrinfo(address_result);
            close(sfd);
            throw syscall_error("connect", sys_call_ret);
        }
    } else {
        freeaddrinfo(address_result);
        close(sfd);
        throw syscall_error("getaddrinfo", errcode);
    }

    return sfd;
}

// Fragment copied from previous task. Used mainly for autoclosing socket.
class Socket {
  private:
    std::shared_ptr<int> _socket_fd{new int(-1), [](int *x) {
                                        if (*x == -1)
                                            return;

                                        ::close(*x);
                                        delete x;
                                    }};

  public:
    int get_fd() const { return *_socket_fd; }

    operator int() const { return *_socket_fd; }

    void close() {
        ::close(*_socket_fd);
        *_socket_fd = -1;
    }

    Socket() = delete;
    // Variable order diffrent than in socket function!!
    // (for deafult values)
    Socket(int type, int domain = AF_INET, int protocol = 0) {
        *_socket_fd = socket(domain, type, protocol);
        if (*_socket_fd < 0) {
            throw syscall_error("socket", *_socket_fd);
        }
    }

    Socket(int fd) {
        *_socket_fd = fd;
        if (fd < 0) {
            throw syscall_error("socket", *_socket_fd);
        }
    }
};

}
#endif /* NET_HPP */