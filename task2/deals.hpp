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

// Helper class to in deal definition.
// Allows for every deal counting function to be defined once.
template<class T>
class FunctionAdder {
  private:
    using func_t = std::function<T()>;
    inline static const func_t zero = func_t([](){ return T(); });
    inline static func_t last = zero;
  public:
    static func_t add(func_t f) {
        last = [f]() { return f() + last(); };
        return f;
    }

    static func_t sum() {
        auto temp = last;
        last = zero;
        return temp;
    }
};

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
        Place::id_t _first_player;
        Place::id_t _loser;
    };

  private:
    // Function for score calculations. 
    std::vector<std::function<int()>> dealf{
        FunctionAdder<int>::add([]() { return 1; }),

        FunctionAdder<int>::add([&, this]() { 
            return std::ranges::count_if(_table, 
                [](auto c){ return c.get_color() == Card::KIER; }); }),

        FunctionAdder<int>::add([&, this]() {
            return 5 * std::ranges::count_if(_table, 
                [](auto c){ return c.get_value() == Card::DAMA; }); }),

        FunctionAdder<int>::add([&, this]() { 
            return 2 * std::ranges::count_if(_table, 
                [](auto c){ return (c.get_value() == Card::WALET || 
                    c.get_value() == Card::KROL); }); }),

        FunctionAdder<int>::add([&, this]() {
            return 18 * std::ranges::count_if(_table, 
                [](auto c){ return (c.get_value() == Card::KROL && 
                    c.get_color() == Card::KIER); }); }),

        FunctionAdder<int>::add([&, this]() {
            return 10 * (_lew_counter == MAX_LEW_COUNT ||
                _lew_counter == 7); }),

        FunctionAdder<int>::sum()
    };

    // Overall game stats
    std::vector<Deck> _first_hands;
    std::vector<LewState> _history;
    std::vector<uint> _scores = std::vector<uint>(4, 0);

    uint _deal;

    // Current lew state
    std::vector<Deck> _hands;
    std::vector<Card> _table;
    Place::id_t _first_player;
    uint _placed_cnt = 0;
    uint _lew_counter = 0;

  public:    
    Deal(uint deal, uint first_player, std::vector<Deck> hands) : 
    _first_hands(hands), _deal(deal), _hands(hands), 
    _table(4, Card(0,0)), _first_player(first_player) {
        if (_hands.size() != PLAYER_CNT || 
            std::ranges::any_of(_hands, [](auto h) { 
                return h.size() != LEW_CNT; 
            })) {
            debuglog << " number of hands: " <<  _hands.size() << "\n";
            for (std::size_t i = 0; i < _hands.size(); i++) {
                debuglog << "[ER] Player " << i << " hand size is " 
                         << _hands[i].size() << "\n";
            }
            throw game_error("Player decks are incorrect.");
        }
    };

    uint get_type() const { return _deal; }
    uint get_lew_cnt() const { return _lew_counter + 1; }
    Place::id_t get_first_player() const { return _first_player; }
    uint get_placed_cnt() const { return _placed_cnt; }

    std::vector<Card> get_table() const { return _table; }
    std::vector<uint> get_scores() const { return _scores; }

    std::vector<Deck> get_first_hands() const { return _first_hands; }
    std::vector<LewState> get_history() const { return _history; }

    Place::id_t get_next_player() const { 
        return (_first_player + _placed_cnt) % PLAYER_CNT; 
    }


    bool put(Place::id_t player, Card card) {
        if (_placed_cnt >= PLAYER_CNT ||
            player != (_first_player + _placed_cnt) % PLAYER_CNT) {
            debuglog << "Gracz " << player 
                     << " dokłada w nie swojej turze" << "\n";
            return false;
        }

        if (player != _first_player && card.get_color() != 
                _table[_first_player].get_color() && 
            _hands[player].has_color(_table[_first_player].get_color())) {
            debuglog << "Gracz " << player << " nie dokłada do koloru" << "\n";
            return false;
        }

        if (!_hands[player].get(card)) {
            debuglog << "Gracz " << player 
                     << " zagrywa nie posiadną karte" << "\n";
            return false;
        }

        _table[player] = card;
        _placed_cnt++;
        return true;
    }

    // Checks if all players gave card. Turn can end normally.
    bool is_done() const {
        return _placed_cnt == PLAYER_CNT;
    }

    std::size_t get_loser() const {
        std::size_t loser = _first_player;
        std::size_t value = _table[_first_player].get_value();
        std::size_t color = _table[_first_player].get_color();

        for (std::size_t i = 0; i < _table.size(); i++) {
            if (color == _table[i].get_color() && 
                value < _table[i].get_value()) {
                loser = i;
                value = _table[i].get_value();
            }
        }

        return loser;
    }

    bool end_lew() {
        if (!is_done()) {
            throw game_error("Ending undone lew");
        }

        auto loser = get_loser();
        _scores[loser] += dealf[_deal]();

        _history.push_back({_table, _first_player, loser});

        _lew_counter++;

        if (_lew_counter == MAX_LEW_COUNT) {
            return true;
        }

        _first_player = loser;
        _placed_cnt = 0;

        return false;
    }

    static uint parse(std::string_view &text) {
        return (uint)parser(deals, text);
    }
};

#endif