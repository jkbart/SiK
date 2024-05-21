#ifndef DEALS_HPP
#define DEALS_HPP

#include "common.hpp"

#include <vector>
#include <optional>
#include <functional>

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
    const uint MAX_LEW_COUNT = 13;

    enum DEAL_t : uint {
        NO_LEW = 0,
        NO_KIER = 1,
        NO_DAMA = 2,
        NO_PANA = 3,
        NO_KIER_KROL = 4,
        NO_7th_LAST_LEW = 5,
        ROZBOJNIK = 6
    };

    inline static const std::vector<std::string> deals{
        "1", "2", "3", "4", "5", "6", "7"
    };

    const std::vector<std::function<int()>> dealf{
        FunctionAdder<int>::add([]() { return 1; }),
        FunctionAdder<int>::add([&, this]() { 
            return std::ranges::count_if(_table, 
                [](auto c){ return c->get_color() == Card::KIER; }); }),
        FunctionAdder<int>::add([&, this]() {
            return 5 * std::ranges::count_if(_table, 
                [](auto c){ return c->get_value() == Card::DAMA; }); }),
        FunctionAdder<int>::add([&, this]() { 
            return 2 * std::ranges::count_if(_table, 
                [](auto c){ return (c->get_value() == Card::WALET || 
                    c->get_value() == Card::KROL); }); }),
        FunctionAdder<int>::add([&, this]() {
            return 18 * std::ranges::count_if(_table, 
                [](auto c){ return (c->get_value() == Card::KROL && 
                    c->get_color() == Card::KIER); }); }),
        FunctionAdder<int>::add([&, this]() {
            return 10 * (_lew_counter == MAX_LEW_COUNT ||
                _lew_counter == 7); }),
        FunctionAdder<int>::sum()
    };

  private:
    std::vector<std::optional<Card>> _table;
    std::vector<int> _scores;

    DEAL_t _deal;
    uint _first_player;
    uint _next_player;
    uint _lew_counter = 0;

  public:    
    Deal(DEAL_t deal, uint first_player) : _table(4, std::nullopt), 
        _scores(4, 0), _deal(deal), _first_player(first_player),
        _next_player(first_player) {};
    Deal(std::string_view &text, bool full_match = false) : 
        Deal((DEAL_t)parser(deals, text), 
            (uint)parser(Place::places, text, full_match)) {}
    Deal(std::string_view &&text, bool full_match = false) : 
        Deal(text, full_match) {}

    void put(Card card) {
        if (_table[_next_player].has_value()) {
            // TODO
            throw new std::runtime_error("ERRORORO");
        }

        _table[_next_player].emplace(card);

        _next_player = (_next_player + 1) % _table.size();
    }

    // Checks if all players gave card. Turn can end normally.
    bool is_done() const {
        return std::ranges::all_of(_table, [](auto c) { return (bool)c; });
    }

    uint get_loser() const {
        if (!is_done()) {
            throw new std::runtime_error("ERRORORO");
        }

        uint loser = _first_player;
        uint value = _table[_first_player]->get_value();
        uint color = _table[_first_player]->get_color();

        for (Card::id_t i = 0; i < _table.size(); i++) {
            if (value < _table[i]->get_value() && 
                color == _table[i]->get_color()) {
                loser = i;
                value = _table[i]->get_value();
            }
        }

        return loser;
    }

    void end_lew() {
        auto loser = get_loser();
        _scores[loser] += dealf[_deal]();

        _first_player = _next_player = loser;
        for (auto &e : _table) {
            e.reset();
        }
    }

    const std::vector<std::optional<Card>>& get_table() { return _table; }

    const std::vector<int>& get_scores() { return _scores; }

    uint get_next_player() const { return _next_player; }

    inline static DEAL_t parse(std::string_view &text) {
        return (DEAL_t)parser(deals, text);
        // for (id_t i = 0; i < deals.size(); i++) {
        //     if (text.starts_with(deals[i])) {
        //         text.remove_prefix(deals[i].size());
        //         return DEAL_t(i);
        //     }
        // } 
        // throw std::runtime_error("ERROROROROOR"); // TODO
    }
};
#endif