// #include "debug.hpp"
// #include "common.hpp"
// #include "deals.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>
#include <cctype>

// using namespace DEBUG_NS;
using namespace std::literals;

std::vector<std::string> tab = {"A", "B", "C", "D"};

uint parse_number_with_maybe_card_behind(std::string_view &text) {
    auto first_char = std::ranges::find_if(text, [](char x) {return std::isalpha(x);});
    std::cout << (std::string)(first_char) << " " << first_char << "\n";
    if (std::ranges::find(tab, (std::string)(first_char)) != tab.end()) {

    }
    std::cout << (std::ranges::find(tab, (std::string)(first_char)) == tab.begin() + 2) << "\n";
    std::cout << (std::ranges::find(tab, (std::string)(first_char)) == tab.begin() + 1) << "\n";
    return 0;
}

int main() {
    auto a = "12C"sv;
    parse_number_with_maybe_card_behind(a);
}