#include "common.hpp"

#include "server_tcp.hpp"

int main(int argc, char *argv[]) {
    SERVER::run_server_tcp(IO::read_port(argv[1]));
}