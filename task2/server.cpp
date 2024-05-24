#include <iostream>
#include <string>
#include <cstring>


int main(int argc, char* argv[]) {
    int check_args = 0;

    uint port = 0;
    std::string file_s;
    uint timeout = 5;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            i++;

            if (i == argc) throw std::invalid_argument("No port value");

            port = std::stoul(argv[i]);
        } else if (!strcmp(argv[i], "-f")) {
            check_args |= 1;
            i++;

            if (i == argc) throw std::invalid_argument("No file name value");
            
            file_s = argv[i];
        } else if (!strcmp(argv[i], "-t")) {
            i++;

            if (i == argc) throw std::invalid_argument("No timeout value");
            
            timeout = std::stoul(argv[i]);
        } else {
            throw std::invalid_argument(
                "Unknown argument " + std::string(argv[i]));
        }
    }

    if (check_args != 1) {
        throw std::invalid_argument("Not all args were specified");
    }
}