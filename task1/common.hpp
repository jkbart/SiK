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

namespace NET {
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

        int get_fd() {
            return _socket_fd;
        }

        ~Socket() {
            /* Stop both reception and transmission. */
            shutdown(_socket_fd, 2);
        }

    };

//     class Port {
//     private:
//         Socket socket;
//     public:
//         void bind_to(const sockaddr &address) {
//             if (bind(_socket_fd, &address, sizeof(sockaddr)) != 0) {
//                 throw std::runtime_error("Couldn't bind socket(" + 
//                     std::to_string(_socket_fd) + ")");
//             }
//         }

//         void connect_to(const sockaddr &server_address) {
//             if (connect(_socket_fd, &server_address, 
//                 (socklen_t) sizeof(server_address))) {
//                 throw std::runtime_error("Couldn't connect socket(" + 
//                     std::to_string(_socket_fd) + ")");
//             }
//         }
//     }

    struct package {
        sockaddr_in client_address;
        std::vector<char> data;
    };

    class UDP_server {
    private:
        Socket _socket;
    public:
        UDP_server(Socket socket, const sockaddr &address) : _socket(socket) {
            if (bind(_socket.get_fd(), &address, sizeof(sockaddr)) != 0) {
                throw std::runtime_error("Couldn't bind socket(" + 
                    std::to_string(_socket.get_fd()) + ")");
            }
        }

        package recv_package() {
            package ans;

            return ans;
        }
    };
}

#endif /* COMMON_HPP */