#include "client_tcp.hpp"

#include "io.hpp"

int main(int argc, char *argv[]) {

    char const *host = argv[1];
    uint16_t port = IO::read_port(argv[2]);

    CLIENT::run_client_tcp(IO::get_server_address(host, port));
}