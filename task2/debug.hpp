#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <cstring>
#include <iostream>
#include <string>
#include <chrono>
#include <source_location>

namespace DEBUG_NS {
#ifdef DEBUG
static constexpr bool debug = true;
#else
static constexpr bool debug = false;
#endif


std::string time_printer(auto begin) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - begin)
                    .count();
    std::string time = std::to_string(ms);
    time.insert(0, 6 - time.size(), '-');
    time.insert(3, 1, '\'');
    return time + "ms";
}

static const auto& start_time() {
    static const auto time = std::chrono::steady_clock::now();
    return time;
}

// static auto get_cnt() {
//     static int cnt = 0;
//     return cnt++;
// }


struct DBG {
    const std::source_location loc;
    DBG(std::source_location location = std::source_location::current()) : loc(location) {}

    template <class... Args> 
    void log(const Args &... args) {
        if constexpr (!debug)
            return;

        std::clog << "[DEBUG][" << time_printer(start_time()) << "]";
        std::clog << "[" << loc.file_name() << ":" << loc.line() << "] ";
        // std::clog << "[" << loc.function_name() << "] ";
        ((std::clog << args << " "), ...);
        std::clog << "\n" << std::flush;

        // get_cnt();
    }
};

// template <class T, class... Args> 
// void DBG_printer(const Helper<T> first, const Args &... args) {
//     if constexpr (!debug)
//         return;

//     std::clog << "[DEBUG][" << time_printer(start_time()) << "]";
//     std::clog << "[" << first.loc.function_name() << "]";
//     std::clog << first.value << " ";
//     ((std::clog << args << " "), ...);
//     std::clog << "\n" << std::flush;

//     // get_cnt();
// }

// void DBG_printer(const std::source_location loc = std::source_location::current()) {
//     static const auto begin = std::chrono::steady_clock::now();
//     if constexpr (!debug)
//         return;

//     std::clog << "[DEBUG][" << time_printer(start_time()) << "]";
//     std::clog << "[" << loc.function_name() << "]";
//     std::clog << "\n" << std::flush;

//     // get_cnt();
// }
} // namespace DEBUG_NS
#endif /* IO_HPP */