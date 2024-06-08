#include <iostream>
#include <string>
#include <cstring>       
#include <fstream>
#include <cstdlib>

#include <netdb.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>

#include "inout.hpp"
#include "network.hpp"
#include "common.hpp"
#include "deals.hpp"
#include "comms.hpp"
#include "exceptions.hpp"

class GameRun {
  private:
    std::vector<Deal> game;
    std::size_t next_deal = 0;
  public:
    GameRun() = delete;
    GameRun(const std::string &filename) {
        std::ifstream file_input;
        file_input.open(filename);

        while(file_input.peek() != EOF) {
            std::string deal_info;
            getline(file_input, deal_info, '\n');
            std::string_view deal_info_sv(deal_info);

            auto deal_type = parser(Deal::deals, deal_info_sv);
            Place first = Place(deal_info_sv, true);

            std::vector<Deck> hands;
            for (int i = 0; i < PLAYER_CNT; i++) {
                std::string hand;
                getline(file_input, hand, '\n');
                hands.emplace_back(hand, true);
            }
            game.emplace_back(deal_type, first, hands);
        }
    }
    bool has_next() { return next_deal < game.size(); }

    Deal get_next() { return game[next_deal++]; }
};

static constexpr int QUEUE_LENGTH = 10;

int main(int argc, char* argv[]) {
    try {
    // Parsing arguments
    int check_args = 0;

    uint16_t port = 0;
    std::string file_s;
    uint timeout = 5;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            i++;

            if (i == argc) throw std::invalid_argument("No port value");

            port = NET::read_port(argv[i]);
        } else if (!strcmp(argv[i], "-f")) {
            check_args |= 1;
            i++;

            if (i == argc) throw std::invalid_argument("No file name value");
            
            file_s = argv[i];
        } else if (!strcmp(argv[i], "-t")) {
            i++;

            if (i == argc) throw std::invalid_argument("No timeout value");
            
            // std does not have function for unsigned int 
            timeout = (uint)std::stoul(argv[i]); 
        } else {
            throw std::invalid_argument(
                "Unknown argument " + std::string(argv[i]));
        }
    }

    if (check_args != 1) {
        throw std::invalid_argument(
            "Not all args were specified, required: -p <port> -f <game_file>");
    }

    // Checking correctness of file.
    GameRun game(file_s);
    if (!game.has_next()) {
        debuglog << "File has no deals!" << "\n";
        return 0;
    }

    // Converting timeout from s to ms.
    timeout *= 1000;

    // Setting up tcp listener
    NET::Socket queue_socket(SOCK_STREAM, AF_INET6, 0);

    // Enabling ipv4 connections
    int no = 0;
    int syscall_ret = setsockopt((int)queue_socket, IPPROTO_IPV6, IPV6_V6ONLY, 
                                 (void *)&no, sizeof(no));
    if (syscall_ret != 0) {
        throw syscall_error("setsockopt(IPV6_V6ONLY)", syscall_ret);
    }

    // Seting up listening socket.
    struct sockaddr_in6 server_address;
    server_address.sin6_family = AF_INET6;  // IPv6
    server_address.sin6_addr = in6addr_any; // Listening on all interfaces.
    server_address.sin6_port = htons(port);

    syscall_ret = bind((int)queue_socket, (struct sockaddr *) &server_address,
                       (socklen_t) sizeof server_address);
    if (syscall_ret < 0) {
        throw syscall_error("bind", syscall_ret);
    }

    syscall_ret = listen((int)queue_socket, QUEUE_LENGTH);
    if (syscall_ret < 0) {
        throw syscall_error("listen", syscall_ret);
    }

    debuglog << "Server listening on address: " 
             << NET::getsockname(queue_socket) << "\n";

    Poller poller;

    std::size_t idx_accept = poller.add((int)queue_socket);
    bool accepting = true;
    poller.get(idx_accept).events = POLLIN;

    using msnger_ptr = std::unique_ptr<MessengerBI>;

    int8_t active_player_cnt = 0;
    std::vector<msnger_ptr> players(PLAYER_CNT);
    std::set<msnger_ptr> rejectors; // Just sending data and closing.
    std::set<msnger_ptr> waiting_for_place;

    Deal deal = game.get_next();
    std::vector<uint> total_score(PLAYER_CNT, 0);
    std::shared_ptr<Reporter> logger(
        new Reporter(dup_fd(STDOUT_FILENO), poller));

    while (poller.size()) {
        int ret = poller.run();
        if (ret < 0) {
            debuglog << "poll returned < 0" << "\n";
            return 1;
        }
        debuglog << "GROUP 0: running poll\n";
        logger->runOUT();

        if (poller.size() == 1 && logger->send_size() == 0) {
            logger.reset();
            break;
        }
        // debuglog << "Poll loop: " << ret << "\n";

        // GROUP 1
        // Handle new connections
        debuglog << "GROUP 1: new connections\n";
        if (accepting && poller.get(idx_accept).revents & POLLIN) {
            int fd = accept(queue_socket, NULL, NULL);

            if (fd >= 0) {
                debuglog << "New client, accepting" << "\n";
                fcntl(fd, F_SETFL, O_NONBLOCK);

                if (active_player_cnt == PLAYER_CNT) {
                    debuglog << "New client, moving to rejectors" << "\n";
                    msnger_ptr messenger(new MessengerBI(fd, poller,
                        NET::getsockname(fd), NET::getpeername(fd), logger));
                    messenger->send_msg(BUSY().get_msg());
                    rejectors.insert(std::move(messenger));
                } else {
                    debuglog << "New client, waiting for place" << "\n";
                    msnger_ptr messenger(new MessengerBI(fd, poller,
                        NET::getsockname(fd), NET::getpeername(fd), logger));
                    messenger->set_timeout(timeout);
                    waiting_for_place.insert(std::move(messenger));
                }
            }
        }

        debuglog << "rejectors_size: " << rejectors.size()
                << "wfp_size: " << waiting_for_place.size() << "\n";

        // GROUP 2
        // handle rejectors.
        debuglog << "GROUP 2: handling rejectors\n";
        for (auto it_main = rejectors.begin(); 
                    it_main != rejectors.end();) {
            // this allows to remove current iterator.
            auto it = it_main++;
            bool skip = false;

            // All incoming messeges are unwelcome.
            for (auto e : (*it)->run()) {
                debuglog << "Rejector got unwanted messege, closing" << "\n";
                rejectors.erase(it);
                skip = true;
                continue;
            }

            if (skip) continue;

            // If it closed ther is no point in continuing sending msgs.
            if ((*it)->closed()) {
                debuglog << "Rejector end closed, closing as well" << "\n";
                rejectors.erase(it);
                continue;
            }

            // If we sended all msgs, we can close connection.
            if (!(*it)->send_size()) {
                debuglog << "Rejector sended last messege, closing" << "\n";
                rejectors.erase(it);
                continue;
            }
        }

        // GROUP 3
        // Handle current players
        debuglog << "GROUP 3: handling current players\n";
        for (uint i = 0; i < PLAYER_CNT; i++) {
            if (players[i].get() == nullptr) continue;

            try {
            for (auto msg : players[i]->run()) {
                if (!matches<TRICK>(msg)) {
                    debuglog << "Player got not TRICK msg, closing" << "\n";
                    players[i].reset();
                    active_player_cnt--;
                    break;
                }

                TRICK trick(msg);
                if (trick._cards.size() != 1) {
                    debuglog << "Player got trick with not one card, closing\n";
                    players[i].reset();
                    active_player_cnt--;
                    break;
                } else if (active_player_cnt != PLAYER_CNT ||
                            !deal.put(i, trick._cards[0])) {
                    if (active_player_cnt != PLAYER_CNT) {
                        debuglog << "Not all players are connected, WRONG\n";
                    } else {
                        debuglog << "Player cannot put this card not, WRONG\n";
                    }
                    players[i]->
                        send_msg(WRONG(deal.get_lew_cnt()).get_msg());
                } else {
                    debuglog << "succesful trick\n";
                    // Sucesful trick, checking if trick ended.
                    players[i]->reset_timeout();

                    if (deal.is_done()) {
                        TAKEN taken(deal.get_lew_cnt(), deal.get_table(), 
                                    Place(deal.get_loser()));
                        for (uint j = 0; j < PLAYER_CNT; j++) {
                            if (players[j].get() != nullptr)
                                players[j]->send_msg(taken.get_msg());
                        }

                        if (deal.end_lew()) {
                            debuglog << "Ending deal" << "\n";
                            auto score = deal.get_scores();
                            for (uint j = 0; j < PLAYER_CNT; j++) {
                                total_score[j] += score[j];
                            }

                            std::vector<std::string> score_msgs = 
                                {SCORE(score).get_msg(), 
                                TOTAL(total_score).get_msg()};

                            for (uint j = 0; j < PLAYER_CNT; j++) {
                                if (players[j].get() == nullptr) continue;
                                players[j]->send_msgs(score_msgs);
                                players[j]->reset_timeout();
                            }

                            if (game.has_next()) {
                                debuglog << "Creating new deal" << "\n";
                                deal = game.get_next();
                                for (uint j = 0; j < PLAYER_CNT; j++) {
                                    if (players[j].get() == nullptr) continue;
                                    players[j]->send_msgs(
                                        get_player_deal_history(deal, j));
                                }
                            } else {
                                debuglog << "Ending game, no new deals" << "\n";
                                for (uint j = 0; j < PLAYER_CNT; j++) {
                                    if (players[j].get() == nullptr) continue;
                                    rejectors.insert(std::move(players[j]));
                                }

                                // Stopping accepting new connections.
                                active_player_cnt = PLAYER_CNT;
                                accepting = false;
                                poller.rm(idx_accept);
                                queue_socket.close();
                                break;
                            }
                        }
                    }

                    // Sending new trick
                    debuglog << "sending next, next player: " 
                             << deal.get_next_player() << "\n";
                    if (players[deal.get_next_player()] != nullptr) {
                        debuglog << "Sending new trick: " 
                                 << next_trick(deal) << "\n";
                        players[deal.get_next_player()]->send_msg(
                            next_trick(deal));
                        players[deal.get_next_player()]->set_timeout(timeout);
                    }
                }
            }
            // Check if we closed current player.
            if (players[i] == nullptr) continue;

            // deal.get_next_player() == i should always be true if timeout 
            // happens.  
            if (players[i]->did_timeout() && deal.get_next_player() == i) {
                players[i]->send_msg(
                    next_trick(deal));
                players[i]->set_timeout(timeout);
            }

            } catch (std::exception &e) {
                debuglog << "Player got msg in wrong format, closing: " 
                         << e.what() << "\n";
                players[i].reset();
                active_player_cnt--;
            }

            if (players[i] && players[i]->closed()) {
                debuglog << "Player " << (std::string)Place(i) 
                         << " end closed, closing as well\n";
                players[i].reset();
                active_player_cnt--;
                continue;
            }
        }

        // GROUP 4
        // Handle clients waiting for place assignment (IAM msg).
        debuglog << "GROUP 4: handling WFP\n";
        for (auto it_main = waiting_for_place.begin(); 
                    it_main != waiting_for_place.end();) {
            // this allows to remove current iterator
            auto it = it_main++;

            bool skip = false;
            bool got_iam = false;
            Place::id_t place = -1; // Initialization to supress compiler warnings.

            try {
            // Check if got IAM, anserw WRONG to trick and close on wrong msg
            for (auto e : (*it)->run()) {
                if (matches<TRICK>(e)) {
                    (*it)->send_msg(
                        WRONG(deal.get_lew_cnt())
                            .get_msg());
                } else if (matches<IAM>(e)) {
                    if (got_iam) {
                        debuglog << "WFP got second iam, closing" << "\n";
                        waiting_for_place.erase(it);
                        skip = true;
                        break;
                    }
                    got_iam = true;
                    IAM iam(e);
                    place = iam._place;
                } else {
                    debuglog << "WFP got unwanted msg, closing" << "\n";
                    waiting_for_place.erase(it);
                    skip = true;
                    break;
                }
            }
            } catch (std::exception& e) {
                debuglog << "WFP got msg in wrong format, closing: " << e.what() << "\n";
                waiting_for_place.erase(it);
                skip = true;
            }

            if (skip) continue;

            if ((*it)->closed()) {
                debuglog << "WFP end closed, closing as well" << "\n";
                waiting_for_place.erase(it);
                continue;
            }

            if (got_iam) {
                if (players[place]) {
                    debuglog << "WFP got IAM for taken place,"
                             << "moving to rejectors" << "\n";
                    std::vector<Place> busy_list;
                    for (int i = 0; i < PLAYER_CNT; i++)
                        if (players[i])
                            busy_list.emplace_back(i);
                    (*it)->send_msg(BUSY(busy_list).get_msg());
                    (*it)->reset_timeout();
                    rejectors.insert(std::move(
                        waiting_for_place.extract(it).value()));
                } else {
                    debuglog << "WFP got IAM for free place, "
                             << "assiging as new player " 
                             << (std::string) Place(place) << "\n";
                    players[place] = std::move(
                        waiting_for_place.extract(it).value());
                    debuglog << "Succefully moved to players ffrom wfp\n";
                    players[place]->send_msgs(
                        get_player_deal_history(deal, place));
                    debuglog << "Succefully sended msgs\n";
                    active_player_cnt++;
                    players[place]->reset_timeout();

                    // Sending trick messege to next player.
                    if (active_player_cnt == PLAYER_CNT 
                        // && !deal.is_done() // Is this && necesary? TODO
                        ) { 
                        players[deal.get_next_player()]->
                            send_if_no_timeout(next_trick(deal));
                        players[deal.get_next_player()]->set_timeout(timeout);
                    }
                }
            } else if (!got_iam && (*it)->did_timeout()) {
                debuglog << "WFP timedout, closing" << "\n";
                waiting_for_place.erase(it);
                continue;
            }
        }

        if (poller.size() == 1 && logger->send_size() == 0) {
            logger.reset();
        }
    }
    } catch (std::exception &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    } 


}

