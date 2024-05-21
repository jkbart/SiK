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
    id_t parse_value(std::string_view &text) {
        for (id_t i = 0; i < values.size(); i++) {
            if (text.starts_with(values[i])) {
                text.remove_prefix(values[i].size());

                return i;
            }
        }

        // TODO: better excpetion
        throw std::runtime_error("NOT FOUND VALUE OF CARD");
    }

    id_t parse_color(std::string_view &text) {
        for (id_t i = 0; i < colors.size(); i++) {
            if (text.starts_with(colors[i])) {
                text.remove_prefix(colors[i].size());

                return i;
            }
        }
        
        // TODO: better excpetion
        throw std::runtime_error("NOT FOUND COLOR OF CARD");
    }

    id_t _value, _color;
  public:
    Card(id_t value, id_t color) : _value(value), _color(color) {}
    Card(std::string_view &text) : Card(parse_value(text), parse_color(text)) {}
    Card(std::string_view &&text) : Card(text) {}

    id_t get_value() const { return _value; }
    id_t get_color() const { return _color; }

    operator std::string() {
        return values[_value] + colors[_color];
    }

    bool operator<=>(const Card&) const = default;
};

struct pole {
    const int a,b;
    pole(int aa, int bb) : a(aa), b(bb) {} 
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
    id_t parse_place(std::string_view &text) {
        for (id_t id = 0; id < places.size(); id++) {
            if (text.starts_with(places[id])) {
                text.remove_prefix(places[id].size());
                return id;
            }
        }

        // TODO: better excpetion
        throw std::runtime_error("PLACE NOT CORRECT FORMAT");
    }

    id_t _place;
    
  public:
    Place(id_t place) : _place(place) {}
    Place(std::string_view &text) : _place(parse_place(text)) {}
    Place(std::string_view &&text) : Place(text) {}

    id_t get_place() const {
        return _place;
    }

    operator std::string() {
        return places[_place];
    }
};

#endif