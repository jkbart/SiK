#ifndef DEBUG_HPP
#define DEBUG_HPP

#include <string>
#include <iostream>
#include <cstring>

namespace DEBUG_NS {
#ifdef DEBUG
static constexpr bool debug = true;
#else
static constexpr bool debug = false;
#endif

static int cnt = 0;

template<class... Args>
void DBG_printer(const Args&... args) {
    if constexpr (!debug)
        return;


    std::clog << "[DEBUG][" << cnt <<"] ";
    ((std::clog << args << " "), ... );
    std::clog << "\n" << std::flush;

    cnt++;
}
} // DEBUG_NS namespace
#endif /* IO_HPP */