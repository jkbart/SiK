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

#include "debug.hpp"
using namespace DEBUG_NS;

namespace NET {

uint16_t read_port(char const *string) {
    unsigned long port = std::stoul(string);
    if (port > UINT16_MAX) {
        throw std::invalid_argument("Incorrect port format: " + std::string(string));
    }
    return (uint16_t) port;
}


std::string getname(int socket, 
                    std::function<int(int, sockaddr*, socklen_t*)> f) {
    sockaddr_storage st;
    socklen_t len;
    if (f(socket, (sockaddr*) &st, &len) != 0) {
        debuglog << strerror(errno) << "\n";
        throw std::runtime_error("getpeername failed");
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
        debuglog << strerror(errno) << "\n";
        throw std::runtime_error("Unknown protocol");
    }

    char buffor[INET6_ADDRSTRLEN];
    if (nullptr == inet_ntop(domain, &st, buffor, INET6_ADDRSTRLEN)) {
        debuglog << strerror(errno) << "\n";
        throw std::runtime_error("inet_ntop failed");
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
    hints.ai_family = AF_INET; //domain;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);

    if (errcode != 0) {
        debuglog << gai_strerror(errno) << "\n";
        throw std::runtime_error("getaddrinfo");
    }

    int sfd = socket(hints.ai_family, hints.ai_socktype, 0); // last argument equal to hints.ai_protocol does not work for some reason.
    if (sfd == -1) {
        freeaddrinfo(address_result);
        debuglog << strerror(errno) << "\n";
        throw std::runtime_error("socket");
    }

    if (hints.ai_family == AF_INET6) {
        debuglog << "Connecting using ipv6\n";
        struct sockaddr_in6 server_address;
        server_address.sin6_family = AF_INET6;  // IPv6
        server_address.sin6_addr =
                ((struct sockaddr_in6 *) (address_result->ai_addr))->sin6_addr;
        server_address.sin6_port = htons(port);
        if (::connect(sfd, (sockaddr*)&server_address, sizeof(sockaddr_in6)) != 0) {
            freeaddrinfo(address_result);
            close(sfd);
            debuglog << strerror(errno) << "\n";
            throw std::runtime_error("connect");
        }
    } else if (hints.ai_family == AF_INET) {
        debuglog << "Connecting using ipv4\n";
        struct sockaddr_in server_address;
        server_address.sin_family = AF_INET;  // IPv4
        server_address.sin_addr =
                ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr;
        server_address.sin_port = htons(port);
        if (::connect(sfd, (sockaddr*)&server_address, sizeof(sockaddr_in)) != 0) {
            freeaddrinfo(address_result);
            close(sfd);
            debuglog << strerror(errno) << "\n";
            throw std::runtime_error("connect");
        }
    } else {
        freeaddrinfo(address_result);
        close(sfd);
        debuglog << strerror(errno) << "\n";
        throw std::runtime_error("getaddrinfo");
    }

    return sfd;
}

// struct sockaddr_in6 get_server_address_ipv6(char const *host, uint16_t port) {
//     struct addrinfo hints;
//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_family = AF_INET6; // IPv6
//     hints.ai_socktype = SOCK_DGRAM;
//     hints.ai_protocol = IPPROTO_UDP;

//     struct addrinfo *address_result;
//     int errcode = getaddrinfo(host, NULL, &hints, &address_result);
//     if (errcode != 0) {
//         fatal("getaddrinfo: %s", gai_strerror(errcode));
//     }

//     struct sockaddr_in6 server_address;
//     server_address.sin6_family = AF_INET6;  // IPv6
//     server_address.sin6_addr =
//             ((struct sockaddr_in6 *) (address_result->ai_addr))->sin6_addr;
//     server_address.sin6_port = htons(port);

//     freeaddrinfo(address_result);

//     return server_address;
// }


// struct ip {
//     virtual int get_type() = 0;
//     virtual sockaddr* get_address() = 0;
//     virtual socklen_t get_length() = 0;
//     virtual void print_to_os(std::ostream& os) = 0;
//     virtual ~ip();
// };

// struct ip4 : public ip {
//     sockaddr_in address;
//     static const socklen_t length = sizeof(sockaddr_in);

//     int get_type() { return AF_INET; }
//     sockaddr* get_address() { return (sockaddr*) &address; }
//     socklen_t get_length() { return length; }
//     void print_to_os(std::ostream& os) {
//         static char name[INET_ADDRSTRLEN + 1];
//         inet_ntop(AF_INET6, &address.sin_addr, name, sizeof(name));
//         os << name;
//     }
// };

// struct ip6 : public ip {
//     sockaddr_in6 address;
//     static const socklen_t length = sizeof(sockaddr_in6);

//     int get_type() { return AF_INET6; }
//     sockaddr* get_address() { return (sockaddr*) &address; }
//     socklen_t get_length() { return length; }
//     void print_to_os(std::ostream& os) {
//         static char name[INET6_ADDRSTRLEN + 1];
//         inet_ntop(AF_INET6, &address.sin6_addr, name, sizeof(name));
//         os << name;
//     }
// };



// std::unique_ptr<ip> get_server_address(char const *host, uint16_t port, int type) {
//     struct addrinfo hints;
//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_family = type;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_protocol = IPPROTO_TCP;

//     struct addrinfo *address_result;
//     int errcode = getaddrinfo(host, NULL, &hints, &address_result);
//     if (errcode != 0) {
//         fatal("getaddrinfo: %s", gai_strerror(errcode));
//     }

//     struct sockaddr_in send_address;
//     send_address.sin_family = AF_INET;   // IPv4
//     send_address.sin_addr.s_addr =       // IP address
//             ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
//     send_address.sin_port = htons(port); // port from the command line

//     freeaddrinfo(address_result);

//     return send_address;
// }

// class NetAddress {
//   private:
//     std::unique_ptr<ip> address;
//   public:
//     NetAddress() = delete;

//     NetAddress(char const *host, uint16_t port, int ip_type) {
//         struct addrinfo hints;
//         memset(&hints, 0, sizeof(struct addrinfo));
//         hints.ai_family = ip_type;
//         hints.ai_socktype = SOCK_STREAM;
//         hints.ai_protocol = IPPROTO_TCP;

//         struct addrinfo *address_result;
//         int errcode = getaddrinfo(host, NULL, &hints, &address_result);
//         if (errcode != 0) {
//             fatal("getaddrinfo: %s", gai_strerror(errcode));
//         }
//     }

//     NetAddress(int ip_type) {
//         if (ip_type == AF_INET6) {
//             address = std::make_unique<ip6>();
//         } else if (ip_type == AF_INET) {
//             address = std::make_unique<ip4>();
//         }
//     }

//     int get_type() { return address->get_type(); }
//     sockaddr* get_address() { return address->get_address(); }
//     socklen_t get_length() { return address->get_length(); }

//     friend std::ostream& operator<< (std::ostream& os, const NetAddress& na) {
//         na.address->print_to_os(os);
//         return os;
//     }
// };

// Fragment copied from previous task.
class Socket {
  private:
    std::shared_ptr<int> _socket_fd{new int(-1), [](int *x) {
                                        if (*x == -1)
                                            return;

                                        ::close(*x);
                                        delete x;
                                    }};

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
            throw std::runtime_error(std::string("Couldn't create socket: ") +
                                     std::strerror(errno));
        }
    }

    Socket(int fd) {
        *_socket_fd = fd;
        if (fd < 0) {
            throw std::runtime_error(std::string("Couldn't create socket: ") +
                                     std::strerror(errno));
        }
    }
};

// NetAddress get_sock_addr(Socket socket, int type) {
//     NetAddress addr(type);
//     socklen_t length_copy = addr.get_length();
//     if (getsockname((int)socket, addr.get_address(), &length_copy) != 0)
//         throw std::runtime_error("getsockname failed");

//     if (length_copy != addr.get_length())
//         throw std::runtime_error("getsockname returned unexpected addr length");

//     return addr;
// }

// void bind(Socket socket, NetAddress addr) {
//     if (::bind((int)socket, addr.get_address(), addr.get_length()) < 0) {
//         throw std::runtime_error("bind failed");
//     }
// }

// void listen(Socket sock, int queue_length) {
//     if (::listen((int)sock, queue_length) < 0) {
//         throw std::runtime_error("listen failed");
//     }
// }

// std::tuple<Socket, NetAddress> accept(Socket sock, int type) {
//     NetAddress addr(type);
//     socklen_t length_copy = addr.get_length();
//     int new_fd = ::accept((int)sock, addr.get_address(), &length_copy);

//     if (new_fd < 0)
//         throw std::runtime_error("accept failed");
//     if (length_copy != addr.get_length())
//         throw std::runtime_error("accept returned unexpected addr length");

//     return std::make_tuple(Socket(new_fd), addr);
// }

}
#endif /* NET_HPP */