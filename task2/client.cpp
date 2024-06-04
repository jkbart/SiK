#include <iostream>
#include <cstring>
#include <string>
#include <memory>
#include <ranges>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "network.hpp"
#include "common.hpp"
#include "comms.hpp"
#include "inout.hpp"

#include "debug.hpp"
using namespace DEBUG_NS;

// Picks card for trick from deck.
Card autopicker(const Deck &deck, std::vector<Card> cards) {
    std::vector<Card> sample;

    if (cards.empty()) {
        sample = deck.list();
    } else if (deck.has_color(cards.front().get_color())) {
        for (auto e : deck.list())
            if (e.get_color() == cards.front().get_color())
                sample.push_back(e);
    } else {
        sample = deck.list();
    }

    if (sample.empty()) {
        throw std::runtime_error("Tried to pick card from empty deck");
    } else {
        return sample[gen() % sample.size()];
    }
}

void autoremover(Deck &deck, std::vector<Card> cards) {
    for (auto e : cards)
        deck.get(e);
}

int main(int argc, char* argv[]) {
    int check_args = 0;

    char  *host;
    uint port;
    int domain = AF_UNSPEC;
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
            port = NET::read_port(argv[i]);
        } else if (!strcmp(argv[i], "-4")) {
            domain = AF_INET;
        } else if (!strcmp(argv[i], "-6")) {
            domain = AF_INET6;
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
        } else if (!strcmp(argv[i], "-a")) {
            is_automatic = true;
        } else {
            throw std::invalid_argument("Unknown argument " + std::string(argv[i]));
        }
    }

    if (check_args != (1 | 1<<1 | 1<<2)) {
        throw std::invalid_argument("Not all args were specified");
    }

    // Connect to server.
    int server_socket = NET::connect(host, port, domain);


    Place my_place(place_s);
    Deck my_deck;
    std::vector<TAKEN> all_takes;
    bool waits_for_trick = false;

    Poller poller;
    std::shared_ptr<Reporter> logger;
    if (is_automatic) { // Will only log if player is automatic.
        logger = std::make_shared<Reporter>(dup(STDOUT_FILENO), poller);
    }

    std::unique_ptr<MessengerBI> server(new MessengerBI(server_socket, poller,
                        NET::getsockname(server_socket), 
                        NET::getpeername(server_socket), logger));
    server->send_msg(IAM(my_place).get_msg());

    std::unique_ptr<MessengerIN> handlerIN(new MessengerIN(
                    dup(STDIN_FILENO), poller, "", "", nullptr, "\n"));
    std::unique_ptr<MessengerOUT> handlerOUT(new MessengerOUT(
                    dup(STDOUT_FILENO), poller, "", "", nullptr, "\n"));

    while (poller.size()) {
        int ret = poller.run();
        if (ret < 0) {
            debuglog << "poll returned < 0" << "\n";
            return 1;
        }

        if (logger) { // Defined if player is automatic.
            logger->runOUT();
        }

        if (server) {
            bool is_closing = false;
            for (auto msg : server->run()) {
                debuglog << "Client recvd: " << std::quoted(msg) << "\n";
                if (is_closing) break;
                try {
                std::unique_ptr<COMMS> comm;
                if (matches<BUSY>(msg)) {
                    debuglog << "matched to BUSY" << "\n";
                    is_closing = true;
                    comm = std::make_unique<BUSY>(msg);
                } else if (matches<DEAL>(msg)) {
                    debuglog << "matched to DEAL" << "\n";
                    my_deck = DEAL(msg)._cards;
                    all_takes.clear();
                    comm = std::make_unique<DEAL>(msg);
                } else if (matches<TRICK>(msg)) {
                    debuglog << "matched to TRICK" << "\n";
                    TRICK trick(msg);
                    if (trick._lew_cnt != all_takes.size() + 1) {
                        debuglog << "Not current lew, skipping\n";
                        continue; // Not current lew.
                    }

                    if (is_automatic) {
                        server->send_msg(TRICK(trick._lew_cnt, 
                            {autopicker(my_deck, trick._cards)}).get_msg());
                    } else {
                        waits_for_trick = true;
                    }
                    comm = std::make_unique<TRICK>(msg);
                } else if (matches<WRONG>(msg)) {
                    debuglog << "matched to WRONG" << "\n";
                    comm = std::make_unique<WRONG>(msg);
                } else if (matches<TAKEN>(msg)) {
                    debuglog << "matched to TAKEN" << "\n";
                    TAKEN taken(msg);
                    if (taken._lew_cnt == all_takes.size() + 1) {
                        all_takes.push_back(TAKEN(msg));
                        autoremover(my_deck, TAKEN(msg)._cards);
                        comm = std::make_unique<TAKEN>(msg);
                    } else {
                        debuglog << "TAKEN was not from last lew\n";
                    }
                } else if (matches<SCORE>(msg)) {
                    debuglog << "matched to SCORE" << "\n";
                    comm = std::make_unique<SCORE>(msg);
                } else if (matches<TOTAL>(msg)) {
                    debuglog << "matched to TOTAL" << "\n";
                    comm = std::make_unique<TOTAL>(msg);
                } else  {
                    // Ignore wrong messeges.
                }
                debuglog << "ISAUTOMATIC " << is_automatic << "\n";
                if (!is_automatic) {
                    debuglog << "printing UI\n";
                    handlerOUT->send_msg(comm->getUI());
                }

                } catch (std::exception &e) {
                    debuglog << e.what() << "\n";
                    // Ignore wrong messeges.
                }
            }

            if (is_closing || ((server->revents() & POLLHUP) && 
                                !(server->revents() & ~POLLIN))) {
                server.reset();
            }
        }

        if (handlerOUT)
            handlerOUT->runOUT();

        if (!server && !handlerOUT->send_size())
            handlerOUT.reset();

        if (!handlerOUT && !server) {
            handlerIN.reset();
        }

        if (handlerIN) {
            for (auto e : handlerIN->runIN()) {
                if (is_automatic) {
                    throw std::runtime_error("Automatic player received input on STDIN");
                }

                if (e == "cards") {
                    handlerOUT->send_msg(
                        list_to_string(my_deck.list(), ", "));
                } else if (e == "tricks") {
                    for (auto &e : all_takes) {
                        if (e._place == my_place) {
                            handlerOUT->send_msg(
                                list_to_string(e._cards, ", "));
                        }
                    }
                } else if (e.starts_with("!")) {
                    try {
                        debuglog << "received card for trick, parsing\n";
                        std::string_view sv(e.begin() + 1, e.end());
                        Card card(sv, true);
                        if (server) {
                            debuglog << "Sending trick to server\n";
                            server->send_msg(
                                TRICK(all_takes.size() + 1, {card}).get_msg());
                        } else {
                            debuglog << "Server already closed!!!\n";
                        }
                    } catch (std::exception &e) {
                        debuglog << e.what() << "\n";
                    }
                } else {
                    // Ignore other messeges?
                }
            }
        }

        poller.print_debug();
    }

}