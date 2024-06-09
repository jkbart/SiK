#include <iostream>
#include <chrono>
#include <iomanip>

#include "common.hpp"
#include "comms.hpp"


int main() {
    for (int i = 1; i <= 7; i++) {
        Deck deck(true);
        auto list = deck.list();

        std::cout << i << "N\n";
        for (int p = 0; p < PLAYER_CNT; p++) {
            for (int j = 0; j < 13; j++) {
                std::cout << (std::string)list[j + p * 13];
            }
            std::cout << "\n";
        }
    }
}