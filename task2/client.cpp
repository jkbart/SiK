#include <iostream>
#include <cstring>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(int argc, char* argv[]) {
    int check_args = 0;

    std::string host;
    uint port;
    int ip_v = AF_UNSPEC;
    std::string place_s;
    bool is_automatic = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            check_args |= 1;
            i++;
            if (i == argc) {
                throw std::invalid_argument("No host ip value");
            }
            host = argv[i];
        } else if (!strcmp(argv[i], "-p")) {
            check_args |= 1<<1;
            i++;
            if (i == argc) {
                throw std::invalid_argument("No host port value");
            }
            port = std::stoul(argv[i]);
        } else if (!strcmp(argv[i], "-4")) {
            ip_v = AF_INET;
        } else if (!strcmp(argv[i], "-6")) {
            ip_v = AF_INET6;
        } else if (!strcmp(argv[i], "-N")) {
            check_args |= 1<<2;
            place_s = "N";
        } else if (!strcmp(argv[i], "-W")) {
            check_args |= 1<<2;
            place_s = "W";
        } else if (!strcmp(argv[i], "-S")) {
            check_args |= 1<<2;
            place_s = "S";
        } else if (!strcmp(argv[i], "-E")) {
            check_args |= 1<<2;
            place_s = "E";
        } else {
            throw std::invalid_argument("Unknown argument " + std::string(argv[i]));
        }
    }

    if (check_args != (1 | 1<<1 | 1<<2)) {
        throw std::invalid_argument("Not all args were specified");
    }


}