#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <exception>
#include <source_location>
#include <iomanip>
#include <cstring>
#include <string>
#include <cerrno>
#include <vector>
#include <cstddef>

using namespace std::string_literals;

class syscall_error : public std::exception {
  protected:
    std::string msg;

  public:
    syscall_error() = delete;

    syscall_error(const std::string &name, int ret, 
        std::source_location loc = std::source_location::current()) {
        msg = "Syscall \""s + name + 
              "\" returned "s + std::to_string(ret) +
              " with errno "s + std::to_string(errno) +
              ": \""s + std::strerror(errno) + 
              "\" at "s + loc.file_name() + ":"s + std::to_string(loc.line());
    }

    const char *what() const throw() {
        return msg.c_str();
    }
};

class parsing_error : public std::exception {
  protected:
    std::string msg;

  public:
    parsing_error() = delete;

    parsing_error(const std::string_view &text, const std::vector<std::string> match, 
        bool full_match, std::source_location loc = 
            std::source_location::current()) {
        msg = "Cannot match (full="s + (full_match ? "1"s : "0"s) + "):\""s + 
              std::string(text) + "\" with one of {"s;
        for (std::size_t i = 0; i < match.size(); i++) {
            if (i != 0) msg += ", "s;
            msg += "\""s + match[i] + "\""s;
        }
        msg += "} at "s + loc.file_name() + ":"s + std::to_string(loc.line());
    }

    parsing_error(const std::string_view &text, const std::string &match,
        bool full_match, std::source_location loc = 
            std::source_location::current()) {
        msg = "Cannot match (full="s + (full_match ? "1"s : "0"s) + "): \""s + 
              std::string(text) + "\" with \""s + match + "\""s;
        msg += " at "s + loc.file_name() + ":"s + std::to_string(loc.line());
    }

    const char *what() const throw() {
        return msg.c_str();
    }
};

class game_error : public std::exception {
  protected:
    std::string msg;

  public:
    game_error() = delete;

    game_error(const std::string &text, 
        std::source_location loc = std::source_location::current()) {
        msg = text + " at "s + loc.file_name() + ":"s + 
              std::to_string(loc.line());
    }

    const char *what() const throw() {
        return msg.c_str();
    }
};


#endif /* EXCEPTIONS_HPP */