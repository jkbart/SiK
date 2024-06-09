#ifndef DEALS_HPP
#define DEALS_HPP

#include "common.hpp"
#include "exceptions.hpp"

#include "debug.hpp"
using namespace DEBUG_NS;

#include <vector>
#include <optional>
#include <functional>
#include <ranges>
#include <cstddef>


static constexpr int PLAYER_CNT = 4;
static constexpr int LEW_CNT = 13;

class Deal {
  public:
    inline static const uint MAX_LEW_COUNT = 13;

    // { // Types of deals and their indexes.
    //     NO_LEW = 0,
    //     NO_KIER = 1,
    //     NO_DAMA = 2,
    //     NO_PANA = 3,
    //     NO_KIER_KROL = 4,
    //     NO_7th_LAST_LEW = 5,
    //     ROZBOJNIK = 6
    // }

    inline static const std::vector<std::string> deals{
        "1", "2", "3", "4", "5", "6", "7"
    };

    struct LewState {
        std::vector<Card> state;
        Place::id_t first_player;
        Place::id_t loser;
    };

  private:
    uint count_points() {
        uint ans = 0;
        if (deal == 0 || deal == 6) {
            ans = 1;
        }

        if (deal == 1 || deal == 6) {
            for (Card c : table)
                ans += (uint)(c.get_color() == Card::KIER);
        }

        if (deal == 2 || deal == 6) {
            for (Card c : table)
                ans += 5 * (uint)(c.get_color() == Card::DAMA);
        }

        if (deal == 3 || deal == 6) {
            for (Card c : table)
                ans += 2 * (uint)(c.get_value() == Card::WALET || 
                    c.get_value() == Card::KROL);
        }

        if (deal == 4 || deal == 6) {
            for (Card c : table)
                ans += 18 * (uint)(c.get_value() == Card::KROL && 
                    c.get_color() == Card::KIER);
        }

        if (deal == 5 || deal == 6) {
            ans += 10 * (uint)(get_lew_cnt() == MAX_LEW_COUNT ||
                get_lew_cnt() == 7);
        }
        return ans;
    }

    // Overall game stats
    std::vector<Deck> first_hands;
    std::vector<LewState> history;
    std::vector<uint> scores = std::vector<uint>(PLAYER_CNT, 0);

    uint deal;

    // Current lew state
    std::vector<Deck> hands;
    std::vector<Card> table;
    Place::id_t first_player;
    uint placed_cnt = 0;
    uint lew_counter = 0;

  public:    
    Deal(uint deal_, uint first_player_, std::vector<Deck> hands_) : 
    first_hands(hands_), deal(deal_), hands(hands_), 
    table(4, Card(0,0)), first_player(first_player_) {
        if (hands.size() != PLAYER_CNT || 
            std::ranges::any_of(hands, [](auto h) { 
                return h.size() != LEW_CNT; // Check if hands have correct size.
            })) {
            debuglog << "[ER] number of hands: " <<  hands.size() << "\n";
            for (std::size_t i = 0; i < hands.size(); i++) {
                debuglog << "[ER] Player " << i << " hand size is " 
                         << hands[i].size() << "\n";
            }
            throw game_error("Player decks are incorrect.");
        }
    };

    uint get_type() const { return deal; }
    uint get_lew_cnt() const { return lew_counter + 1; }
    Place::id_t get_first_player() const { return first_player; }
    uint get_placed_cnt() const { return placed_cnt; }

    std::vector<Card> get_table() const { return table; }
    std::vector<uint> get_scores() const { return scores; }

    std::vector<Deck> get_first_hands() const { return first_hands; }
    std::vector<LewState> get_history() const { return history; }

    Place::id_t get_next_player() const { 
        return (first_player + placed_cnt) % PLAYER_CNT; 
    }


    bool put(Place::id_t player, Card card) {
        if (placed_cnt >= PLAYER_CNT) {
            debuglog << "PLayer " << player << " is adding when lew\n";
            return false;
        }

        if (player != get_next_player()) {
            debuglog << "PLayer " << player << " is not adding in his turn\n";
            return false;
        }

        if (player != first_player &&
            card.get_color() != table[first_player].get_color() && 
            hands[player].has_color(table[first_player].get_color())) {
            debuglog << "Player " << player << " is not adding to color\n";
            return false;
        }

        if (!hands[player].get(card)) {
            debuglog << "Player " << player 
                     << " is adding card he does NOT have\n";
            return false;
        }

        table[player] = card;
        placed_cnt++;
        return true;
    }

    // Checks if all players gave card. Turn can end normally.
    bool is_done() const {
        return placed_cnt == PLAYER_CNT;
    }

    std::size_t get_loser() const {
        std::size_t loser = first_player;
        Card::id_t value = table[first_player].get_value();
        Card::id_t color = table[first_player].get_color();

        for (std::size_t i = 0; i < table.size(); i++) {
            if (color == table[i].get_color() && 
                value < table[i].get_value()) {
                loser = i;
                value = table[i].get_value();
            }
        }

        return loser;
    }

    // Returns true if this deal has lews left and false otherwise.
    bool end_lew() {
        if (!is_done()) {
            throw game_error("Ending undone lew");
        }

        auto loser = get_loser();
        // scores[loser] += dealf[deal]();
        scores[loser] += count_points();

        history.push_back({table, first_player, loser});

        lew_counter++;

        if (lew_counter == MAX_LEW_COUNT) {
            return true;
        }

        first_player = loser;
        placed_cnt = 0;

        return false;
    }

    // Returns id of deal type from string_view.
    static uint parse(std::string_view &text) {
        return (uint)parser(deals, text);
    }
};

#endif