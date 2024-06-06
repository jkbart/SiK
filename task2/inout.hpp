#ifndef IO_HPP
#define IO_HPP

#include <unistd.h>
#include <poll.h>

#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <queue>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <iomanip>
#include <cstddef>

#include "exceptions.hpp"

#include "debug.hpp"
using namespace DEBUG_NS;

// Duplicates fd.
int dup_fd(int fd) {
    int ret = dup(fd);
    if (ret == -1)
        throw syscall_error("dup", ret);
    return ret;
}

// Converts \n and \r to \\n and \\r
std::string special_printer(const std::string &text) {
    std::string ret;
    for (char c : text) {
        if (c == '\n') {
            ret += "\\n";
        } else if (c == '\r') {
            ret += "\\r";
        } else {
            ret += c;
        }
    }
    return ret;
}

class Poller {
  private:
    inline static constexpr int INVALID_FD = -1;
    // all 3 vectors have the same size at all times
    std::vector<struct pollfd> poll_descriptors;
    std::vector<int> timeouts; // in ms. 0 == no timeout
    std::vector<bool> _did_timeout;

  public:
    Poller() {}

    struct pollfd& get(int i) { return poll_descriptors[i]; }

    void set_timeout(int i, int ms) { 
        timeouts[i] = ms; 
        _did_timeout[i] = false; 
    }

    int get_timeout(int i) { return timeouts[i]; }
    bool did_timeout(int i) { return _did_timeout[i]; }

    uint size() {
        uint ret = 0;
        for (const auto& e : poll_descriptors)
            if (e.fd != INVALID_FD)
                ret++;
        return ret;
    }

    int add(int fd) {
        // Reuse old index if possible.
        for (std::size_t i = 0; i < poll_descriptors.size(); i++) {
            if (poll_descriptors[i].fd == INVALID_FD) {
                poll_descriptors[i].fd = fd;
                debuglog << "POLLER: " << "Adding new fd " << fd 
                         << " on index " << i << "\n";
                return i;
            }
        }

        // Create new index.
        poll_descriptors.push_back({.fd = fd, .events = 0, .revents = 0});
        timeouts.push_back(0);
        _did_timeout.push_back(false);

        debuglog << "POLLER: " << "Adding new fd " << fd << " on index " 
                 << poll_descriptors.size() - 1 << "\n";

        return poll_descriptors.size() - 1;
    }

    void rm(int i) {
        debuglog << "POLLER: " << "removing index " << i << "\n";
        poll_descriptors[i] = {.fd = INVALID_FD, .events = 0, .revents = 0};
        timeouts[i] = 0;
    }

    int run() {
        for (std::size_t i = 0; i < poll_descriptors.size(); i++) {
            if (poll_descriptors[i].fd == INVALID_FD) {
                poll_descriptors[i].events = 0;
                poll_descriptors[i].revents = 0;
            }
        }
        int timeout = -1;
        for (std::size_t i = 0; i < timeouts.size(); i++) {
            if (timeouts[i] > 0 && (timeout == -1 || timeouts[i] < timeout))
                timeout = timeouts[i];
            _did_timeout[i] = false;
            poll_descriptors[i].revents = 0;
        }

        auto start = std::chrono::steady_clock::now();
        int poll_status = poll(poll_descriptors.data(), 
                               poll_descriptors.size(), timeout);
        int duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();

        // Check if descriptor timedout.
        for (std::size_t i = 0; i < timeouts.size(); i++) {
            if (timeouts[i] == 0)
                continue;
            timeouts[i] = std::max(0, timeouts[i] - duration);
            _did_timeout[i] = (timeouts[i] == 0);
        }

        debuglog << "POLL: POLLSTTATUS " << poll_status << " TIMEOUT " << timeout << "\n";
        print_debug();

        return poll_status;
    }

    void print_debug() {
        debuglog << "POLL: READ WRITE ERR " << POLLIN << " " << POLLOUT << " " << POLLERR << "\n";
        debuglog << "POLL: fd events revents did_timeout\n";
        for (std::size_t i = 0; i < timeouts.size(); i++) {
            if (poll_descriptors[i].fd != INVALID_FD) {
                debuglog << "POLL: " 
                         << poll_descriptors[i].fd << " "
                         << poll_descriptors[i].events << " "
                         << poll_descriptors[i].revents << " "
                         << _did_timeout[i] << "\n";
            }
        }
    }
};

class Messenger;
class MessengerIN;
class MessengerOUT;
class MessengerBI;
class Reporter;

class Messenger {
  protected:
    const int _desc;
    Poller &_poller;
    int _poll_idx;
    std::shared_ptr<Reporter> _logger;
    std::string _my_name;
    std::string _peer_name;
    std::string _delim;
    bool _is_closed = false;

  public:
    Messenger(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger = nullptr,
        std::string delim = "\r\n");

    int revents();

    bool did_timeout();
    void set_timeout(int ms);
    void reset_timeout();
    int get_fd();
    bool closed();

    virtual ~Messenger();
};

class MessengerIN : virtual public Messenger {
  public:
    static constexpr int buffor_size = 1000;
    static constexpr int readlen = 100;

  protected:
    std::vector<char> _buffor;
    size_t _size = 0;

  public:
    MessengerIN(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger = nullptr,
        std::string delim = "\r\n");

    void read();

    std::vector<std::string> get_msgs();

    std::vector<std::string> runIN();
    
    virtual ~MessengerIN();
};

class MessengerOUT : virtual public Messenger {
  protected:
    std::queue<std::string> _buffor_to_send;
    size_t _sended = 0;
  public:
    MessengerOUT(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger = nullptr,
        std::string delim = "\r\n");

    bool write();

    void send_msg(const std::string &msg);

    void send_msgs(const std::vector<std::string> &msgs);
    void send_if_no_timeout(const std::string &msg);

    int send_size();

    void runOUT();

    virtual ~MessengerOUT() = default;
};

class MessengerBI : public MessengerIN, public MessengerOUT {
  public:
    MessengerBI(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger = nullptr,
        std::string delim = "\r\n");

    std::vector<std::string> run();
};

class Reporter : public MessengerOUT {
  private:
    // Place for local address.

    std::string get_time();

  public:
    Reporter(int desc, Poller &poller) : 
        Messenger(desc, poller, "", ""), // No names for peer and myself..
        MessengerOUT(desc, poller, "", "") {}

    void transmission(std::string msg, std::string from, std::string to);

    void sendedTo(const std::string &me, const std::string &peer, 
                  const std::string &msg);

    void recvdFrom(const std::string &me, const std::string &peer, 
                  const std::string &msg);
};

Messenger::Messenger(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger,
        std::string delim) : _desc(desc), _poller(poller), _logger(logger), 
    _my_name(my_name), _peer_name(peer_name), _delim(delim) {
    _poll_idx = _poller.add(desc);
    _poller.get(_poll_idx).events = 0;
}

int Messenger::revents() { return _poller.get(_poll_idx).revents; } 

bool Messenger::did_timeout() { return _poller.did_timeout(_poll_idx); }
void Messenger::set_timeout(int ms) { _poller.set_timeout(_poll_idx, ms); }
void Messenger::reset_timeout() { return set_timeout(0); }
int Messenger::get_fd() { return _desc; }
bool Messenger::closed() {return _is_closed; }

Messenger::~Messenger() {
    _poller.rm(_poll_idx);
    close(_desc);
}


MessengerIN::MessengerIN(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger,
        std::string delim) : 
    Messenger(desc, poller, my_name, peer_name, logger, delim), 
    _buffor(buffor_size) {
    _poller.get(_poll_idx).events |= POLLIN;
}

void MessengerIN::read() {
    ssize_t len_read = ::read(_desc, _buffor.data() + _size, readlen);

    if (len_read < 0) {
        throw syscall_error("read", len_read);
    } else if (len_read == 0) { 
        _is_closed = true;
        return;
    }
    _size += len_read;
}

std::vector<std::string> MessengerIN::get_msgs() {
    read();

    std::vector<std::string> msgs;

    while (true) {
        auto sep = std::ranges::search(
            _buffor.begin() , _buffor.begin() + _size,
            _delim.begin(), _delim.end());

        if (sep.empty()) { break; }

        msgs.emplace_back(_buffor.begin(), sep.begin());
        if (_logger)
            _logger->recvdFrom(_my_name, _peer_name,
                std::string(_buffor.begin(), sep.begin()) + 
                    special_printer(_delim));

        _size -= std::distance(_buffor.begin(), sep.end());
        _buffor.erase(_buffor.begin(), sep.end());
    }

    _buffor.resize(buffor_size);
    if (_size > buffor_size - readlen) {
        if (_logger)
            _logger->recvdFrom(_my_name, _peer_name, special_printer(
                std::string(_buffor.begin(), _buffor.begin() + _size)));
        throw std::overflow_error("Buffor on desc " + std::to_string(_desc) + 
            "about to overflow. Incoming messege to long");
    }

    for (auto &e : msgs) {
        debuglog << "MSNGER rcvd: " << std::quoted(e) << "\n";
    }

    return msgs;
}

std::vector<std::string> MessengerIN::runIN() {
    if (revents() & (POLLIN | POLLERR)) {
        return get_msgs();
    }

    return {};
}

MessengerIN::~MessengerIN() {
    if (_logger && _size > 0)
        _logger->recvdFrom(_my_name, _peer_name, special_printer(
            std::string(_buffor.begin(), _buffor.begin() + _size)));
}

MessengerOUT::MessengerOUT(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger,
        std::string delim) : 
    Messenger(desc, poller, my_name, peer_name, logger, delim) {}

bool MessengerOUT::write() {
    if (_buffor_to_send.empty()) {
        return false;
    }

    ssize_t len_write = 
        ::write(_desc, _buffor_to_send.front().data() + _sended, 
            _buffor_to_send.front().size() - _sended);

    if (len_write < 0) {
        throw syscall_error("write", len_write);
    } else if (len_write == 0) { 
        throw syscall_error("write == 0", len_write);
    }

    _sended += len_write;
    if (_sended == _buffor_to_send.front().size()) {
        if (_logger) {
            _buffor_to_send.front().resize(
                _buffor_to_send.front().size() - _delim.size()); // removing \r\n
            _logger->sendedTo(_my_name, _peer_name, 
                _buffor_to_send.front() + special_printer(_delim));
        }
        _buffor_to_send.pop();
        _sended = 0;
        if (_buffor_to_send.empty()) {
            _poller.get(_poll_idx).events &= (~POLLOUT);
        }
        return true;
    }

    return false;
}

void MessengerOUT::send_msg(const std::string &msg) {
    _poller.get(_poll_idx).events |= POLLOUT;
    _buffor_to_send.push(msg + _delim);
}

void MessengerOUT::send_msgs(const std::vector<std::string> &msgs) {
    for (const auto &e : msgs) {
        send_msg(e);
    }
}

void MessengerOUT::send_if_no_timeout(const std::string &msg) {
    if (_poller.get_timeout(_poll_idx) == 0) {
        send_msg(msg);
    }
}

int MessengerOUT::send_size() { return _buffor_to_send.size(); }

void MessengerOUT::runOUT() { if (revents() & POLLOUT) write(); }

MessengerBI::MessengerBI(int desc, Poller &poller, std::string my_name, 
        std::string peer_name, std::shared_ptr<Reporter> logger,
        std::string delim) : 
    Messenger(desc, poller, my_name, peer_name, logger, delim),
    MessengerIN(desc, poller, my_name, peer_name, logger, delim),
    MessengerOUT(desc, poller, my_name, peer_name, logger, delim) {}

std::vector<std::string> MessengerBI::run() {
    // Every option except for POLLIN and POLLOUT.
    if (revents() & (POLLERR | POLLPRI | POLLRDHUP | POLLHUP | POLLNVAL)) {
        _is_closed = true;
    }

    runOUT();
    return runIN();
}

std::string Reporter::get_time() {
    // std::format is in g++ from version 13.
    // Until then we need to use put_time and string_streams
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto milis = std::chrono::duration_cast<std::chrono::milliseconds>
                                (now.time_since_epoch()).count() % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%dT%H:%M:%S.")
        << std::setfill('0') << std::setw(3) << milis;
    return ss.str();
}

void Reporter::transmission(std::string msg, std::string from, std::string to) {
    send_msg("[" +  from + "," + to + "," + get_time() + "] " + msg);
}

void Reporter::sendedTo(const std::string &me, const std::string &peer, 
                        const std::string &msg) {
    transmission(msg, me, peer);
}

void Reporter::recvdFrom(const std::string &me, const std::string &peer, 
                        const std::string &msg) {
    transmission(msg, peer, me);
}


// class Messenger {
//   private:
//     inline static const std::string separator = "\r\n";
//     static constexpr int buffor_size = 1000;
//     static constexpr int readlen = 100;

//     std::vector<char> _buffor;
//     size_t _size = 0;

//     std::queue<std::string> _buffor_to_send;
//     size_t _sended = 0;

//     Poller &_poller;
//     int _poll_idx;
//   public:
//     const int _desc;
//     Messenger(int desc, Poller &poller) : _desc(desc), _poller(poller), 
//         _buffor(buffor_size) {
//         _poll_idx = _poller.add(desc);
//         _poller.get(_poll_idx).events = POLLIN;
//     }

//     void read() {
//         ssize_t len_read = ::read(_desc, _buffor.data() + _size, readlen);

//         if (len_read < 0) {
//             throw std::runtime_error("read < 0");
//         } else if (len_read == 0) { 
//             throw std::runtime_error("read == 0");
//         }
//         _size += len_read;
//     }

//     bool write() {
//         if (_buffor_to_send.empty()) {
//             return false;
//         }

//         ssize_t len_write = 
//             ::write(_desc, _buffor_to_send.front().data() + _sended, 
//                 _buffor_to_send.front().size() - _sended);

//         if (len_write < 0) {
//             throw std::runtime_error("write < 0");
//         } else if (len_write == 0) { 
//             throw std::runtime_error("write == 0");
//         }

//         _sended += len_write;
//         if (_sended == _buffor_to_send.front().size()) {
//             _buffor_to_send.pop();
//             _sended = 0;
//             if (_buffor_to_send.empty()) {
//                 _poller.get(_poll_idx).events = POLLIN;
//             }
//             return true;
//             // _poller.get(_poll_idx).events = POLLIN;
//         }

//         return false;
//     }

//     std::vector<std::string> get_msgs() {
//         read();

//         std::vector<std::string> msgs;

//         while (true) {
//             auto sep = std::ranges::search(
//                 _buffor.begin() , _buffor.begin() + _size,
//                 separator.begin(), separator.end());

//             if (sep.empty()) { break; }

//             msgs.emplace_back(_buffor.begin(), sep.begin());

//             _size -= std::distance(_buffor.begin(), sep.end());
//             _buffor.erase(_buffor.begin(), sep.end());
//         }

//         _buffor.resize(buffor_size);
//         if (_size > buffor_size - readlen) {
//             throw std::overflow_error("Buffor on desc " + std::to_string(_desc) + 
//                 "about to overflow. Incoming messege to long");
//         }

//         for (auto &e : msgs) {
//             debuglog << "MSNGER rcvd: " << e << "\n";
//         }

//         return msgs;
//     }

//     void send_msg(const std::string &msg) {
//         _poller.get(_poll_idx).events = POLLOUT | POLLIN;
//         _buffor_to_send.push(msg + "\r\n");
//     }

//     void send_msgs(const std::vector<std::string> &msgs) {
//         _poller.get(_poll_idx).events = POLLOUT | POLLIN;
//         for (const auto &e : msgs) {
//             _buffor_to_send.push(e + "\r\n");
//         }
//     }

//     void send_if_no_timeout(const std::string &msg) {
//         if (_poller.get_timeout(_poll_idx) == 0) {
//             send_msg(msg);
//         }
//     }

//     std::vector<std::string> run() {
//         if (revents() & POLLOUT) write();
//         if (revents() & (POLLIN | POLLERR)) {
//             return get_msgs();
//         }

//         return {};
//     }

//     int revents() { return _poller.get(_poll_idx).revents; } 

//     bool did_timeout() { return _poller.did_timeout(_poll_idx); }
//     void set_timeout(int ms) { _poller.set_timeout(_poll_idx, ms); }
//     void reset_timeout() { return set_timeout(0); }

//     int send_size() { return _buffor_to_send.size(); }

//     ~Messenger() {
//         _poller.rm(_poll_idx);
//         close(_desc);
//     }
// };

#endif /* IO_HPP */