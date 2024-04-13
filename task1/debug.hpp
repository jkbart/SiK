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

template<class Arg>
void printer_in_hex(Arg arg) {
    std::string type_name = std::string(typeid(Arg).name());
    std:: cout << type_name << " :";
    for (size_t i = type_name.size(); i < 20; i++) std::cout << " ";
    std::cout << "\"";
    char *ptr = (char *)&arg;
    for (size_t i = 0; i < sizeof(Arg); i++) {
        if (i != 0) std::cout << " ";
        printf("%x", *ptr);
        ptr++;
    }
    std::cout << "\"\n";
}

template<class Arg>
Arg read_single_var(char *buffor) {
    Arg var;
    std::memcpy(&var, buffor, sizeof(Arg));
    return var;
}

template<class... Args>
void DBG_printer(Args... args) {
    if constexpr (!debug)
        return;
    std::clog << "[DEBUG]\n";
    (
        (std::clog << "----[DBG_printer]" << args), 
        ...
    );
}

template<class... Args>
void DBG_printer_in_hex(Args... args) {
    if constexpr (!debug)
        return;
    std::clog << "[DEBUG]\n";
    (
        ((std::clog << "----[DBG_printer]"), printer_in_hex(args)), 
        ...
    );
}

}
#endif /* IO_HPP */