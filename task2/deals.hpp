#ifndef DEALS_HPP
#define DEALS_HPP

#include "common.hpp"

#include <vector>
#include <optional>

enum DEAL_t : int8_t {
    NO_LEW = 1,
    NO_KIER = 2,
    NO_DAMA = 3,
    NO_PANA = 4,
    NO_KIER_KROL = 5,
    NO_7th_LAST_LEW = 6,
    ROZBOJNIK = 7
};

class BaseDeal {
  protected:
    std::vector<std::optional<Card>> _table;
    std::vector<int> _scores;

    uint _first_player;
    uint _next_player;
  public:
    BaseDeal(uint first_player) : _table(4, std::nullopt), _scores(4, 0), 
    _first_player(first_player), _next_player(first_player) {};

    void put(Card card) {
        if (_table[_next_player].has_value()) {
            // TODO
            throw new std::runtime_error("ERRORORO");
        }

        _table[_next_player].emplace(card);

        _next_player = (_next_player + 1) % _table.size();
    }

    virtual void end_lew() = 0;

    const std::vector<std::optional<Card>>& get_table() {
        return _table;
    }

    const std::vector<int>& get_scores() {
        return _scores;
    }

    uint get_next_player() {
        return _next_player;
    }

    virtual ~BaseDeal();
};

template<DEAL_t type>
class Deal;

template<> 
class Deal<NO_LEW> : public BaseDeal {
    void end_lew() {
        for (auto e : _table) {
            if (!e.has_value()) {
                // TODO
                throw new std::runtime_error("ERRORORO");
            }
        }

        uint loser = _first_player;
        uint value = _table[_first_player]->_value;
        uint color = _table[_first_player]->_color;

        for (auto i = 0; i < _table.size(); i++) {
            if (value < _table[i]->_value && color == _table[i]->_color) {
                loser = i;
                value = _table[i]->_value;
            }
        }

        _scores[loser] += 1;
    }

};

#endif