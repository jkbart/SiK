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

  public:

    const id_t _value, _color;

    Card(id_t value, id_t color) : _value(value), _color(color) {}
    Card(std::string_view &text) : 
    _value(parse_value(text)), _color(parse_color(text)) {}
    Card(std::string_view &&text) : Card(text) {}

    operator std::string() {
        return values[_value] + colors[_color];
    }
};

class Deck {
  private:
    std::set<Card> _deck;

  public:
    Deck(bool full) {
        if (full) {
            for (auto value = 0; value < Card::values.size(); value++) {
                for (auto color = 0; color < Card::colors.size(); color++) {
                    _deck.insert(Card(value, color));
                }
            }
        }
    }

    bool get(Card card) {
        return _deck.erase(card);
    }

    bool add(Card card) {
        auto is_added = std::get<1>(_deck.insert(card));
        return is_added;
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
        for (auto id = 0; id < places.size(); id++) {
            if (text.starts_with(places[id])) {
                text.remove_prefix(places[id].size());
                return id;
            }
        }

        // TODO: better excpetion
        throw std::runtime_error("PLACE NOT CORRECT FORMAT");
    }
  public:

    const id_t _place;

    Place(id_t place) : _place(place) {}
    Place(std::string_view &text) : _place(parse_place(text)) {}
    Place(std::string_view &&text) : Place(text) {}

    operator std::string() {
        return places[_place];
    }
};

#endif