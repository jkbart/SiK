#ifndef COMMS_HPP
#define COMMS_HPP

#include "debug.hpp"
#include "common.hpp"
#include "deals.hpp"
#include "exceptions.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <exception>
#include <tuple>
#include <algorithm>
#include <charconv>
#include <ostream>

class COMMS {
  protected:
    const std::string _msg;
    COMMS(std::string msg) : _msg(msg) {} // Making class abstract.
    inline static std::string_view rem_prefix(std::string &msg, const std::string &prefix) {
        std::string_view ans = msg;

        if (!ans.starts_with(prefix))
            throw parsing_error(ans, prefix, false);

        ans.remove_prefix(prefix.size());
        return ans;
    }

  public:
    std::string get_msg() const {
        return _msg;
    }

    virtual std::string getUI() const = 0;

    // virtual std::string_view get_prefix() const = 0;

    virtual ~COMMS() {};
};

class IAM : public COMMS {
  public:
    inline const static std::string _prefix = "IAM";
    const Place _place;

    IAM(Place place) : 
        COMMS(_prefix + (std::string)place), _place(place) {}

    IAM(std::string msg, std::string_view text) :
        COMMS(msg), _place(text, true) {}

    IAM(std::string msg) : IAM(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "New player wants to be placed at " + (std::string)Place(_place);
    }
};

class BUSY : public COMMS {
  public:
    inline const static std::string _prefix = "BUSY";
    const std::vector<Place> _places;

    std::vector<Place> all_busy() {
        std::vector<Place> ret;
        for (std::size_t i = 0; i < Place::places.size(); i++) {
            ret.emplace_back(i);
        }
        return ret;
    }

    BUSY() : 
        COMMS(_prefix + list_to_string(all_busy())), _places(all_busy()) {}

    BUSY(std::vector<Place> places) : 
        COMMS(_prefix + list_to_string(places)), _places(places) {}

    BUSY(std::string msg, std::string_view text) : 
        COMMS(msg), _places(parse_list<Place>(text, true)) {}

    BUSY(std::string msg) : BUSY(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "Place busy, list of busy places received: " + 
            list_to_string(_places, ", ");
    }
};


class DEAL : public COMMS {
  public:
    inline const static std::string _prefix = "DEAL";
    const uint _deal;
    const Place _place;
    const std::vector<Card> _cards;

    DEAL(uint deal, Place place, std::vector<Card> cards) : 
        COMMS(_prefix + Deal::deals[deal] + (std::string)place + 
            list_to_string(cards)), _deal(deal), _place(place), _cards(cards) {}

    DEAL(std::string msg, std::string_view text) :
        COMMS(msg), _deal(Deal::parse(text)), _place(text), 
        _cards(parse_list<Card>(text, true)) {}

    DEAL(std::string msg) : DEAL(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "New deal " + std::to_string(_deal) + ": starting place " + 
            (std::string)_place + ", your cards: " + 
            list_to_string(_cards, ", ") + ".";
    }
};

uint parse_number_with_maybe_card_behind(std::string_view &text) {
    std::string_view sv = text;
    for (std::size_t i = 0; i < text.size() && !Card::valid(sv) &&
                    std::isdigit(text[i]); i++, sv.remove_prefix(1)) {}
    std::string_view number = text;
    number.remove_suffix(sv.size());

    if (number.size() == 0) {
        throw parsing_error(text, "uint(?Card)", false);
    }

    text.remove_prefix(number.size());

    int ans = 0;
    std::from_chars(number.begin(), number.end(), ans);
    return ans;
}

class TRICK : public COMMS {
  public:
    inline const static std::string _prefix = "TRICK";
    const uint _lew_cnt;
    const std::vector<Card> _cards;

    TRICK(uint lew_cnt, std::vector<Card> cards) : 
        COMMS(_prefix + std::to_string(lew_cnt) + list_to_string(cards)),
        _lew_cnt(lew_cnt), _cards(cards) {}

    TRICK(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)), 
        _cards(parse_list<Card>(text, true)) {}

    TRICK(std::string msg) : TRICK(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "Trick: (" + std::to_string(_lew_cnt) + ") " + 
            list_to_string(_cards, ", ");
    }
};

class WRONG : public COMMS {
  public:
    inline const static std::string _prefix = "WRONG";
    const uint _lew_cnt;

    WRONG(uint lew_cnt) : 
        COMMS(_prefix + std::to_string(lew_cnt)),
        _lew_cnt(lew_cnt) {}

    WRONG(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)) {}

    WRONG(std::string msg) : WRONG(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "Wrong message received in trick " + std::to_string(_lew_cnt) + 
            ".";
    }
};

class TAKEN : public COMMS {
  public:
    inline const static std::string _prefix = "TAKEN";
    const uint _lew_cnt;
    const std::vector<Card> _cards;
    const Place _place;

    TAKEN(uint lew_cnt, std::vector<Card> cards, Place place) : 
        COMMS(_prefix + std::to_string(lew_cnt) + list_to_string(cards) + 
            (std::string)place), 
        _lew_cnt(lew_cnt), _cards(cards), _place(place) {
        if (_cards.size() != PLAYER_CNT) {
            throw game_error("wrong number of cards in TAKEN");
        }

    }

    TAKEN(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)),
        _cards(parse_list<Card>(text)), _place(text, true) {
        if (_cards.size() != PLAYER_CNT) {
            throw game_error("wrong number of cards in TAKEN");
        }
    }

    TAKEN(std::string msg) : TAKEN(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "A trick " + std::to_string(_lew_cnt) + " is taken by " + 
            (std::string)_place + list_to_string(_cards, ", ");
    }
};

std::vector<uint> get_scores(std::string_view &text, bool full_match = false) {
    auto text_copy = text; // copy for error handling.
    auto table_size = Place::places.size();

    std::vector<uint> scores(table_size, 0);
    std::vector<bool> seen(table_size, false);

    while (table_size--) {
        Place p(text);
        if (seen[p.get_place()]) {
            throw game_error("score got 2 scores for same place");
        }

        scores[p.get_place()] = parser_uint(text);
    }

    if (!text.empty()) {
        throw parsing_error(text_copy, "scores", full_match);
    }

    return scores;
}

std::string scores_to_string(const std::vector<uint>& scores) {
    std::string ret = "";
    for (uint i = 0; i < scores.size(); i++) {
        ret += (Place::places[i]) + std::to_string(scores[i]);
    }
    return ret;
}

std::string scores_to_string_UI(const std::vector<uint>& scores) {
    std::string ret = "";
    for (uint i = 0; i < scores.size(); i++) {
        if (i != 0) ret += "\n";
        ret += (Place::places[i]) + " | " + std::to_string(scores[i]);
    }
    return ret;
}

class SCORE : public COMMS {
  public:
    inline const static std::string _prefix = "SCORE";
    const std::vector<uint> _scores;

    SCORE(std::vector<uint> scores) : 
        COMMS(_prefix + scores_to_string(scores)), _scores(scores) {}

    SCORE(std::string msg, std::string_view text) : COMMS(msg), 
        _scores(get_scores(text, true)) {}

    SCORE(std::string msg) : SCORE(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "The scores are:\n" + scores_to_string_UI(_scores);
    }
};

class TOTAL : public COMMS {
  public:
    inline const static std::string _prefix = "TOTAL";
    const std::vector<uint> _scores;

    TOTAL(std::vector<uint> scores) : 
        COMMS(_prefix + scores_to_string(scores)), _scores(scores) {}

    TOTAL(std::string msg, std::string_view text) : COMMS(msg), 
        _scores(get_scores(text, true)) {}

    TOTAL(std::string msg) : 
        TOTAL(msg, rem_prefix(msg, _prefix)) {}

    std::string getUI() const {
        return "The total scores are:\n" + scores_to_string_UI(_scores);
    }
};

// Helper functions
template<class T>
requires std::derived_from<T, COMMS>
std::string get_prefix() {
    return T::_prefix;
}

template<class T>
requires std::derived_from<T, COMMS>
bool matches(const std::string &e) {
    return e.starts_with(get_prefix<T>());
}

// Functions integrating deals and comms.
std::vector<std::string> get_player_deal_history(const Deal &deal, uint player) {
    std::vector<std::string> ret;
    auto history = deal.get_history();

    uint first_player = history.empty() ? deal.get_first_player() : 
                                          history[0]._first_player;

    ret.push_back(DEAL(deal.get_type(), Place(first_player),
             deal.get_first_hands()[player].list()).get_msg());

    for (std::size_t lew_cnt = 0; lew_cnt < history.size(); lew_cnt++) {
        std::vector<Card> list;
        for (std::size_t i = 0; i < PLAYER_CNT; i++) {
            list.push_back(history[lew_cnt]
                .state[(i + history[lew_cnt]._first_player) % PLAYER_CNT]);
        }
        ret.push_back(TAKEN(lew_cnt + 1, list, history[lew_cnt]._loser)
                           .get_msg());
    }
    return ret;
}

std::string next_trick(const Deal &deal) {
    std::vector<Card> list;
    for (std::size_t i = 0; i < deal.get_placed_cnt(); i++) {
        list.push_back(
            deal.get_table()[(deal.get_first_player() + i) % PLAYER_CNT]);
    }
    return TRICK(deal.get_lew_cnt(), list).get_msg();
}

#endif
