#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <cstring>
#include <iostream>
#include <string>
#include <chrono>
#include <source_location>
#include <iomanip>

namespace DEBUG_NS {
#ifdef DEBUG
static constexpr bool debug = true;
#else
static constexpr bool debug = false;
#endif

std::string time_printer() {
    static const auto start_time = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time)
                    .count();
    std::string time = std::to_string(ms);
    time.insert(0, 7 - time.size(), '.');
    time.insert(4, 1, '\'');
    time.insert(1, 1, '\'');
    return time + "ms";
}

struct LogStream {
    const std::string name;
    LogStream(std::string name_) : name(name_) {}

    struct Annotated {
        Annotated(LogStream& s, std::source_location loc = 
                                    std::source_location::current()) {
            *this << "[" << s.name << "][" 
                  << time_printer() << "][" << std::setfill('.') 
                  << std::setw(11) << loc.file_name() << ":" 
                  << std::setw(3) << loc.line() << "] ";
        }
    };

    template<class T>
    friend Annotated operator<<(Annotated a, T msg) {
        std::clog << msg;
        return a;
    }
};

// Global logging stream.
static LogStream debuglog_("DBG");

// Macro to fully stop evaluating functions in LogStream when not debugging.
#define debuglog if constexpr (debug) debuglog_

} // namespace DEBUG_NS
#endif /* DEBUG_HPP */