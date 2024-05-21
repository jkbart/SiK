#ifndef COMMON_HPP
#define COMMON_HPP

#include "debug.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <exception>
#include <set>
#include <tuple>
#include <algorithm>
#include <random>
#include <ranges>


std::mt19937 gen{std::random_device{}()};

template<class T>
std::vector<T> parse_list(std::string_view &text, bool full_match = false) {
        std::vector<T> ret;

        while (!text.empty()) {
            T last(text);
            if (std::find(ret.begin(), ret.end(), last) != ret.end()) {
                throw std::runtime_error("IN PARSE TEXT NOT CORRECT FORMAT");
            }
            ret.push_back(last);
        }

        return ret;
}

auto parser(const std::vector<std::string> &range, std::string_view &text, 
    bool full_match = false) {
    for (auto i = 0; i < range.size(); i++) {
        if (text.starts_with(range[i])) {
            if (full_match && text.size() != range[i].size())
                break;
            text.remove_prefix(range[i].size());
            return i;
        }
    }
    // TODO
    throw std::runtime_error("NO MATCH IN ALL CASES");
}

auto parser_from_behind(const std::vector<std::string> &range, std::string_view &text, 
    bool full_match = false) {
    for (auto i = 0; i < range.size(); i++) {
        if (text.ends_with(range[i])) {
            if (full_match && text.size() != range[i].size())
                break;
            text.remove_suffix(range[i].size());
            return i;
        }
    }
    // TODO
    throw std::runtime_error("NO MATCH IN ALL CASES");
}

bool matches(const std::vector<std::string> &range, std::string_view &text, 
    bool full_match = false) {
    for (auto i = 0; i < range.size(); i++) {
        if (text.starts_with(range[i])) {
            if (full_match && text.size() != range[i].size())
                break;
            text.remove_prefix(range[i].size());
            return true;
        }
    }

    return false;
}

// Reads up to 11 digits to avoid overflowing.
uint parser_uint(std::string_view &text, bool full_match = false) {
    uint ans = 0;
    bool check = false;

    for (auto i = 0; !text.empty(); i++, text.remove_prefix(1)) {
        if (!std::isdigit(text[0]) || i == 10)
            break;
        check = true;
        ans = (10 * ans) + (text[0] - '0');
    }

    if (!check || (!text.empty() && full_match)) {
        throw std::runtime_error("ERROR IN UINT PARSING");
    }

    return ans;
}

template<class T>
    requires std::is_convertible_v<T, std::string>
std::string list_to_string(std::vector<T> &list, std::string delim = "") {
    std::string ans = "";
    for (auto i = 0; i < list.size(); i++) {
        ans += list[i];
        if (i != list.size() - 1) {
            ans += delim;
        }
    }
    return ans;
}
class Card {
  public:
    // Id is index in vector.
    inline static const std::vector<std::string> values{
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
    };

    inline static const std::vector<std::string> colors{
        "C", "D", "H", "S"
    };

    using id_t = std::vector<std::string>::size_type;

    enum VALUES : id_t {
        n2 = 0,
        n3 = 1,
        n4 = 2,
        n5 = 3,
        n6 = 4,
        n7 = 5,
        n8 = 6,
        n9 = 7,
        n10 = 8,
        WALET = 9,
        DAMA = 10,
        KROL = 11,
        AS = 12
    };

    enum COLORS : id_t {
        TREFL = 0,
        KARO = 1,
        KIER = 2,
        PIK = 3
    };

  private:
    id_t _value, _color;

  public:
    Card(id_t value, id_t color) : _value(value), _color(color) {}
    Card(std::string_view &text, bool full_match = false) : 
        Card(parser(values, text), parser(colors, text, full_match)) {}
    Card(std::string_view &&text, bool full_match = false) : 
        Card(text, full_match) {}

    id_t get_value() const { return _value; }
    id_t get_color() const { return _color; }

    operator std::string() {
        return values[_value] + colors[_color];
    }

    inline static bool valid(std::string_view text, bool full_match = false) {
        return (matches(values, text) && matches(colors, text, full_match));
    }

    bool operator<=>(const Card&) const = default;
};

class Deck {
  private:
    std::vector<Card> _deck;

  public:
    Deck(bool full = false) {
        if (full) {
            for (id_t value = 0; value < Card::values.size(); value++) {
                for (id_t color = 0; color < Card::colors.size(); color++) {
                    _deck.push_back(Card(value, color));
                }
            }

            std::ranges::shuffle(_deck, gen);
        }
    }

    bool get(Card card) {
        auto it = std::ranges::find(_deck, card);
        if (it == std::end(_deck)) {
            return false;
        }
        _deck.erase(it);
        return true;
    }

    bool add(Card card) {
        if (std::ranges::find(_deck, card) != std::end(_deck)) {
            return false;
        }

        _deck.push_back(card);
        return true;
    }
};

class Place {
  public:
    inline static const std::vector<std::string> places{
        "N", "E", "S", "W"
    };

    using id_t = std::vector<std::string>::size_type;

  private:
    id_t _place;
    
  public:
    Place(id_t place) : _place(place) {}
    Place(std::string_view &text, bool full_match = false) : 
        _place(parser(places, text, full_match)) {}
    Place(std::string_view &&text, bool full_match = false) : 
        Place(text, full_match) {}

    id_t get_place() const {
        return _place;
    }

    operator std::string() {
        return places[_place];
    }

    inline static bool valid(std::string_view text, bool full_match = false) {
        return matches(places, text, full_match);
    }
};

#endif