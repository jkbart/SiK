#ifndef COMMS_HPP
#define COMMS_HPP

#include "debug.hpp"
#include "common.hpp"
#include "deals.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <exception>
#include <tuple>
#include <algorithm>
#include <charconv>

class COMMS {
  protected:
    const std::string _msg;
    COMMS(std::string msg) : _msg(msg) {} // Making class abstract.
    inline static std::string_view rem_prefix(std::string &msg, const std::string &prefix) {
        std::string_view ans = msg;

        if (!ans.starts_with(prefix))
            throw std::runtime_error("Incorrect prefix: " + msg + ", " + prefix);

        ans.remove_prefix(prefix.size());
        return ans;
    }

  public:
    void send() {
        // send _msg;
    }

    virtual std::string_view get_prefix() = 0;

    bool matches(const std::string &text) {
        return text.starts_with(get_prefix());
    }

    virtual ~COMMS();
};

class IAM : public COMMS {
  public:
    inline const static std::string _prefix = "IAM";
    const Place _place;

    IAM(Place place) : 
        COMMS(_prefix + (std::string)place), _place(place) {}

    IAM(std::string msg, std::string_view text) :
        COMMS(msg), _place(text, true) {}

    IAM(std::string msg) : 
        IAM(msg, rem_prefix(msg, _prefix)) {}
};

class BUSY : public COMMS {
  public:
    inline const static std::string _prefix = "BUSY";
    const std::vector<Place> _places;

    BUSY(std::vector<Place> places) : 
        COMMS(_prefix + list_to_string(places)), _places(places) {}

    BUSY(std::string msg, std::string_view text) : 
        COMMS(msg), _places(parse_list<Place>(text, true)) {}

    BUSY(std::string msg) : 
        BUSY(msg, rem_prefix(msg, _prefix)) {}
};


class DEAL : public COMMS {
  public:
    inline const static std::string _prefix = "DEAL";
    const Deal::DEAL_t _deal;
    const Place _place;
    const std::vector<Card> _cards;

    DEAL(Deal::DEAL_t deal, Place place, std::vector<Card> cards) : 
        COMMS(_prefix + (std::string)place + list_to_string(cards)), 
        _deal(deal), _place(place), _cards(cards) {}

    DEAL(std::string msg, std::string_view text) :
        COMMS(msg), _deal(Deal::parse(text)), _place(text), 
        _cards(parse_list<Card>(text, true)) {}

    DEAL(std::string msg) : 
        DEAL(msg, rem_prefix(msg, _prefix)) {}
};

uint parse_number_with_maybe_card_behind(std::string_view &text) {
    std::string_view sv = text;
    for (int i = 0; i < text.size() && !Card::valid(sv) && std::isdigit(text[i]); i++, sv.remove_prefix(1)) {}
    std::string_view number = text;
    number.remove_suffix(sv.size());
    text.remove_prefix(number.size());
    int ans;
    std::from_chars(number.begin(), number.end(), ans);
    return ans;
}

class TRICK : public COMMS {
  public:
    inline const static std::string _prefix = "TRICK";
    const uint _lew_cnt;
    const std::vector<Card> _cards;

    TRICK(Deal::DEAL_t deal, uint lew_cnt, std::vector<Card> cards) : 
        COMMS(_prefix + std::to_string(lew_cnt) + list_to_string(cards)),
        _lew_cnt(lew_cnt), _cards(cards) {}

    TRICK(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)), 
        _cards(parse_list<Card>(text, true)) {}

    TRICK(std::string msg) : 
        TRICK(msg, rem_prefix(msg, _prefix)) {}
};

class WRONG : public COMMS {
  public:
    inline const static std::string _prefix = "TRICK";
    const uint _lew_cnt;

    WRONG(Deal::DEAL_t deal, uint lew_cnt) : 
        COMMS(_prefix + std::to_string(lew_cnt)),
        _lew_cnt(lew_cnt) {}

    WRONG(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)) {}

    WRONG(std::string msg) : 
        WRONG(msg, rem_prefix(msg, _prefix)) {}
};

class TAKEN : public COMMS {
  private:

  public:
    inline const static std::string _prefix = "TRICK";
    const uint _lew_cnt;
    const std::vector<Card> _cards;
    const Place _place;

    TAKEN(Deal::DEAL_t deal, uint lew_cnt, std::vector<Card> cards, Place place) : 
        COMMS(_prefix + std::to_string(lew_cnt)), _lew_cnt(lew_cnt), _cards(cards), _place(place) {}

    TAKEN(std::string msg, std::string_view text) : COMMS(msg), 
        _lew_cnt(parse_number_with_maybe_card_behind(text)),
        _cards(parse_list<Card>(text, false)), _place(text, true) {}

    TAKEN(std::string msg) : 
        TAKEN(msg, rem_prefix(msg, _prefix)) {}
};

// SCORE<miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów>\r\n
// TOTAL<miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów><miejsce przy stole klienta><liczba punktów>\r\n

#endif
