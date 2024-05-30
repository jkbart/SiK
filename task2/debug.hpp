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
    time.insert(0, 7 - time.size(), '.');
    time.insert(4, 1, '\'');
    time.insert(1, 1, '\'');
    return time + "ms";
}

static const auto start_time = std::chrono::steady_clock::now();


// Source: https://quuxplusone.github.io/blog/2020/02/12/source-location/
struct DBGStream {
    const std::string _name;
    DBGStream(std::string name) : _name(name) {}

    struct Annotated {
        /*IMPLICIT*/ Annotated(DBGStream& s,
                  std::source_location loc = std::source_location::current())
        {
            *this << "[" << s._name << "][" 
                  << time_printer(start_time) << "]["
                  << loc.file_name() << ":" << loc.line() << "] ";
        }
    };

    template<class T>
    friend Annotated operator<<(Annotated a, T msg) {
        if constexpr (!debug) {
            return a;
        }
        std::clog << msg;
        return a;
    }
};

static DBGStream debuglog("DBG");
} // namespace DEBUG_NS
#endif /* DEBUG_HPP */