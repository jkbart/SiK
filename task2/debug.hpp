#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <cstring>
#include <iostream>
#include <string>
#include <chrono>

namespace DEBUG_NS {
#ifdef DEBUG
static constexpr bool debug = true;
#else
static constexpr bool debug = false;
#endif

static int cnt = 0;

std::string time_printer(auto begin) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - begin)
                    .count();
    std::string time = std::to_string(ms);
    time.insert(0, 6 - time.size(), ' ');
    time.insert(3, 1, '\'');
    return time + "ms";
}

template <class... Args> void DBG_printer(const Args &... args) {
    static const auto begin = std::chrono::steady_clock::now();
    if constexpr (!debug)
        return;

    std::clog << "[DEBUG][" << time_printer(begin) << "] ";
    ((std::clog << args << " "), ...);
    std::clog << "\n" << std::flush;

    cnt++;
}
} // namespace DEBUG_NS
#endif /* IO_HPP */