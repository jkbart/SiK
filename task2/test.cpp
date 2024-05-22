// #include "debug.hpp"
// #include "common.hpp"
// #include "deals.hpp"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>
#include <cctype>

#include "common.hpp"
#include "comms.hpp"

int main() {
    std::string wrong = "WRONG10";
    std::string taken = "TAKEN102C3C4C5CN";
    std::string score = "SCOREN1W2E6S12";

    WRONG a1(wrong);
    TAKEN a12(taken);
    SCORE a122(score);
    std::cout << list_to_string(a122._scores) << "\n";
}