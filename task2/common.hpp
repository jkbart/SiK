#ifndef COMMON_HPP
#define COMMON_HPP

#include <vector>
#include <string>
#include <string_view>
#include <exception>
#include <set>
#include <tuple>
#include <algorithm>
#include <random>
#include <ranges>
#include <limits.h>
#include <cstddef>

#include "exceptions.hpp"
#include "debug.hpp"
using namespace DEBUG_NS;

// Global random generator.
std::mt19937 gen{std::random_device{}()};

// Helper functions for parsing.
bool matches(const std::vector<std::string> &range, std::string_view &text, 
    bool full_match = false) {
    for (const auto &e : range) {
        if (text.starts_with(e)) {
            if (full_match && text.size() != e.size())
                break;
            text.remove_prefix(e.size());
            return true;
        }
    }

    return false;
}

template<class T>
std::vector<T> parse_list(std::string_view &text, bool full_match = false) {
    std::vector<T> ret;

    while (!text.empty()) {
        if (!full_match && !T::valid(text))
            return ret;

        T last(text);

        // Checks if there are no duplicates.
        if (std::ranges::find(ret, last) != ret.end()) {
            throw game_error("there are duplicates in list to parse");
        }
        ret.push_back(last);
    }

    return ret;
}

// Reads up maximal possible possible uint while avoiding overflow.
uint parser_uint(std::string_view &text, bool full_match = false) {
    auto text_copy = text; // copy for error handling.
    uint ans = 0;
    bool check = false;

    while(!text.empty()) {
        if (!std::isdigit(text[0]) || (UINT_MAX - (text[0] - '0')) / 10 < ans)
            break;
        check = true;

        ans = (10 * ans) + (text[0] - '0');
        text.remove_prefix(1);
    }

    if (!check || (!text.empty() && full_match)) {
        throw parsing_error(text_copy, "uint", full_match);
    }

    return ans;
}

template<class T>
    requires std::is_convertible_v<T, std::string>
std::string list_to_string(const std::vector<T> &list, std::string delim = "") {
    std::string ans = "";
    for (std::size_t i = 0; i < list.size(); i++) {
        if (i != 0) ans += delim;
        ans += static_cast<std::string>(list[i]);
    }
    return ans;
}


template<class T>
    requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
std::string list_to_string(const std::vector<T> &list, std::string delim = "") {
    std::string ans = "";
    for (std::size_t i = 0; i < list.size(); i++) {
        if (i != 0) ans += delim;
        ans += std::to_string(list[i]);
    }
    return ans;
}

std::size_t parser(const std::vector<std::string> &range, std::string_view &text, 
            bool full_match = false) {
    for (std::size_t i = 0; i < range.size(); i++) {
        if (text.starts_with(range[i])) {
            if (full_match && text.size() != range[i].size())
                break;
            text.remove_prefix(range[i].size());
            return i;
        }
    }

    throw parsing_error(text, range, full_match);
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

    using id_t = std::size_t;

    enum VALUES : id_t {
        n2 = 0, n3 = 1, n4 = 2, n5 = 3, n6 = 4, n7 = 5, n8 = 6, n9 = 7, n10 = 8, 
        WALET = 9, DAMA = 10, KROL = 11, AS = 12
    };

    enum COLORS : id_t {
        TREFL = 0, KARO = 1, KIER = 2, PIK = 3
    };

  private:
    id_t value, color;

  public:
    Card(id_t value_, id_t color_) : value(value_), color(color_) {}
    Card(std::string_view &text, bool full_match = false) : 
        value(parser(values, text)), color(parser(colors, text, full_match)) {}
    Card(std::string_view &&text, bool full_match = false) : 
        Card(text, full_match) {}

    id_t get_value() const { return value; }
    id_t get_color() const { return color; }

    operator std::string() const {
        return values[value] + colors[color];
    }

    inline static bool valid(std::string_view text, bool full_match = false) {
        return (matches(values, text) && matches(colors, text, full_match));
    }

    bool operator<=>(const Card&) const = default;
};

class Deck {
  private:
    std::vector<Card> deck;

  public:

    Deck(std::vector<Card> deck_) : deck(deck_) {}
    Deck(std::string_view &text, bool full_match = false) : 
        deck(parse_list<Card>(text, full_match)) {}
    Deck(std::string_view &&text, bool full_match = false) : 
        Deck(text, full_match) {}

    Deck(bool full = false) {
        if (full) {
            for (id_t value = 0; value < Card::values.size(); value++) {
                for (id_t color = 0; color < Card::colors.size(); color++) {
                    deck.push_back(Card(value, color));
                }
            }

            std::ranges::shuffle(deck, gen);
        }
    }

    bool has_color(Card::id_t color) const {
        return std::ranges::any_of(deck, [color](auto c) { 
                return c.get_color() == color; 
            });
    }

    bool has(Card card) const {
        auto it = std::ranges::find(deck, card);
        if (it == std::end(deck)) {
            return false;
        }
        return true;
    }

    bool get(Card card) {
        auto it = std::ranges::find(deck, card);
        if (it == std::end(deck)) {
            return false;
        }
        deck.erase(it);
        return true;
    }

    bool add(Card card) {
        if (std::ranges::find(deck, card) != std::end(deck)) {
            return false;
        }

        deck.push_back(card);
        return true;
    }

    std::size_t size() { return deck.size(); }
    std::vector<Card> list() const { return deck; }
};

class Place {
  public:
    inline static const std::vector<std::string> places{
        "N", "E", "S", "W"
    };

    inline static const int player_count = (int)places.size();

    using id_t = std::size_t;

  private:
    id_t place;
    
  public:
    Place(id_t place_) : place(place_) {}
    Place(std::string_view &text, bool full_match = false) : 
        place(parser(places, text, full_match)) {}
    Place(std::string_view &&text, bool full_match = false) : 
        Place(text, full_match) {}

    id_t get_place() const {
        return place;
    }

    operator std::string() const {
        return places[place];
    }

    operator id_t() const {
        return place;
    }

    inline static bool valid(std::string_view text, bool full_match = false) {
        return matches(places, text, full_match);
    }

    bool operator==(const Place&) const = default;
};

#endif