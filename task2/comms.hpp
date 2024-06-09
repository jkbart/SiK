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
    const std::string msg;
    COMMS(std::string msg_) : msg(msg_) {} // Making class abstract.
    inline static std::string_view rem_prefix(std::string &msg_, 
                                              const std::string &prefix) {
        std::string_view ans = msg_;

        if (!ans.starts_with(prefix))
            throw parsing_error(ans, prefix, false);

        ans.remove_prefix(prefix.size());
        return ans;
    }

  public:
    std::string get_msg() const { return msg; }
    virtual std::string getUI() const = 0;

    virtual ~COMMS() {};
};

class IAM : public COMMS {
  public:
    inline const static std::string prefix = "IAM";
    const Place place;

    IAM(Place place_) : 
        COMMS(prefix + (std::string)place_), place(place_) {}

    IAM(std::string msg_, std::string_view text) :
        COMMS(msg_), place(text, true) {}

    IAM(std::string msg_) : IAM(msg_, rem_prefix(msg_, prefix)) {}

    // This function is undefined in moodle and unused.
    std::string getUI() const {
        return "New player wants to be placed at " + 
            (std::string)Place(place) + ".";
    }
};

class BUSY : public COMMS {
  public:
    inline const static std::string prefix = "BUSY";
    const std::vector<Place> places;

    std::vector<Place> all_busy() {
        std::vector<Place> ret;
        for (std::size_t i = 0; i < Place::places.size(); i++) {
            ret.emplace_back(i);
        }
        return ret;
    }

    BUSY() : 
        COMMS(prefix + list_to_string(all_busy())), places(all_busy()) {}

    BUSY(std::vector<Place> places_) : 
        COMMS(prefix + list_to_string(places_)), places(places_) {}

    BUSY(std::string msg_, std::string_view text) : 
        COMMS(msg_), places(parse_list<Place>(text, true)) {}

    BUSY(std::string msg_) : BUSY(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "Place busy, list of busy places received: " + 
            list_to_string(places, ", ") + ".";
    }
};


class DEAL : public COMMS {
  public:
    inline const static std::string prefix = "DEAL";
    const uint deal;
    const Place place;
    const std::vector<Card> cards;

    DEAL(uint deal_, Place place_, std::vector<Card> cards_) : 
        COMMS(prefix + Deal::deals[deal_] + (std::string)place_ + 
            list_to_string(cards_)), deal(deal_), place(place_), cards(cards_) {}

    DEAL(std::string msg_, std::string_view text) :
        COMMS(msg_), deal(Deal::parse(text)), place(text), 
        cards(parse_list<Card>(text, true)) {}

    DEAL(std::string msg_) : DEAL(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "New deal " + Deal::deals[deal] + ": staring place " + 
            (std::string)place + ", your cards: " + 
            list_to_string(cards, ", ") + ".";
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
    inline const static std::string prefix = "TRICK";
    const uint lew_cnt;
    const std::vector<Card> cards;

    TRICK(uint lew_cnt_, std::vector<Card> cards_) : 
        COMMS(prefix + std::to_string(lew_cnt_) + list_to_string(cards_)),
        lew_cnt(lew_cnt_), cards(cards_) {}

    TRICK(std::string msg_, std::string_view text) : COMMS(msg_), 
        lew_cnt(parse_number_with_maybe_card_behind(text)), 
        cards(parse_list<Card>(text, true)) {}

    TRICK(std::string msg_) : TRICK(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "Trick: (" + std::to_string(lew_cnt) + ") " + 
            list_to_string(cards, ", ");
    }
};

class WRONG : public COMMS {
  public:
    inline const static std::string prefix = "WRONG";
    const uint lew_cnt;

    WRONG(uint lew_cnt_) : 
        COMMS(prefix + std::to_string(lew_cnt_)),
        lew_cnt(lew_cnt_) {}

    WRONG(std::string msg_, std::string_view text) : COMMS(msg_), 
        lew_cnt(parse_number_with_maybe_card_behind(text)) {}

    WRONG(std::string msg_) : WRONG(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "Wrong message received in trick " + std::to_string(lew_cnt) + 
            ".";
    }
};

class TAKEN : public COMMS {
  public:
    inline const static std::string prefix = "TAKEN";
    const uint lew_cnt;
    const std::vector<Card> cards;
    const Place place;

    TAKEN(uint lew_cnt_, std::vector<Card> cards_, Place place_) : 
        COMMS(prefix + std::to_string(lew_cnt_) + list_to_string(cards_) + 
            (std::string)place_), 
        lew_cnt(lew_cnt_), cards(cards_), place(place_) {
        if (cards.size() != PLAYER_CNT) {
            throw game_error("wrong number of cards in TAKEN");
        }

    }

    TAKEN(std::string msg_, std::string_view text) : COMMS(msg_), 
        lew_cnt(parse_number_with_maybe_card_behind(text)),
        cards(parse_list<Card>(text)), place(text, true) {
        if (cards.size() != PLAYER_CNT) {
            throw game_error("wrong number of cards in TAKEN");
        }
    }

    TAKEN(std::string msg_) : TAKEN(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "A trick " + std::to_string(lew_cnt) + " is taken by " + 
            (std::string)place + ", cards " + list_to_string(cards, ", ") + 
            ".";
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
    inline const static std::string prefix = "SCORE";
    const std::vector<uint> scores;

    SCORE(std::vector<uint> scores_) : 
        COMMS(prefix + scores_to_string(scores_)), scores(scores_) {}

    SCORE(std::string msg_, std::string_view text) : COMMS(msg_), 
        scores(get_scores(text, true)) {}

    SCORE(std::string msg_) : SCORE(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "The scores are:\n" + scores_to_string_UI(scores);
    }
};

class TOTAL : public COMMS {
  public:
    inline const static std::string prefix = "TOTAL";
    const std::vector<uint> scores;

    TOTAL(std::vector<uint> scores_) : 
        COMMS(prefix + scores_to_string(scores_)), scores(scores_) {}

    TOTAL(std::string msg_, std::string_view text) : COMMS(msg_), 
        scores(get_scores(text, true)) {}

    TOTAL(std::string msg_) : 
        TOTAL(msg_, rem_prefix(msg_, prefix)) {}

    std::string getUI() const {
        return "The total scores are:\n" + scores_to_string_UI(scores);
    }
};

// Helper functions
template<class T>
    requires std::derived_from<T, COMMS>
std::string get_prefix() {
    return T::prefix;
}

template<class T>
    requires std::derived_from<T, COMMS>
bool matches(const std::string &e) {
    return e.starts_with(get_prefix<T>());
}

// Functions integrating deals and comms.
std::vector<std::string> get_player_deal_history(const Deal &deal, 
                                                 Place::id_t player) {
    std::vector<std::string> ret;
    auto history = deal.get_history();

    Place::id_t first_player = history.empty() ? deal.get_first_player() : 
                                          history[0].first_player;

    ret.push_back(DEAL(deal.get_type(), Place(first_player),
             deal.get_first_hands()[player].list()).get_msg());

    for (uint lew_cnt = 0; lew_cnt < (uint)history.size(); lew_cnt++) {
        std::vector<Card> list;
        for (Place::id_t i = 0; i < PLAYER_CNT; i++) {
            list.push_back(history[lew_cnt]
                .state[(i + history[lew_cnt].first_player) % PLAYER_CNT]);
        }
        ret.push_back(TAKEN(lew_cnt + 1, list, history[lew_cnt].loser)
                           .get_msg());
    }
    return ret;
}

std::string next_trick(const Deal &deal) {
    std::vector<Card> list;
    for (Place::id_t i = 0; i < deal.get_placed_cnt(); i++) {
        list.push_back(
            deal.get_table()[(deal.get_first_player() + i) % PLAYER_CNT]);
    }
    return TRICK(deal.get_lew_cnt(), list).get_msg();
}

#endif
