#ifndef INOUT_HPP
#define INOUT_HPP

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
    constexpr bool show_new_line = false;
    if (!show_new_line) {
        return text;
    } 

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
    ret += "\n";
    return ret;
}

// Seperated definition from implemntation due to circular dependecies.

class Poller {
  private:
    inline static constexpr int INVALID_FD = -1;
    // All 3 vectors have the same size at all times.
    std::vector<struct pollfd> poll_descriptors;
    std::vector<int> timeouts; // In ms. 0 <==> no timeout
    std::vector<bool> did_timeout_;

  public:
    Poller();

    struct pollfd& get(std::size_t i);

    void set_timeout(std::size_t i, int ms);
    int get_timeout(std::size_t i);
    bool did_timeout(std::size_t i);

    uint size();

    std::size_t add(int fd);
    void rm(std::size_t i);

    int run();

    void print_debug();
};

class Messenger;
class MessengerIN;
class MessengerOUT;
class MessengerBI;
class Reporter;

class Messenger {
  protected:
    const int desc;
    Poller &poller;
    std::size_t poll_idx;
    std::shared_ptr<Reporter> logger;
    std::string my_name;
    std::string peer_name;
    std::string delim;
    bool is_closed = false;
    short int last_flags = 0; // for unset/reset_flags

  public:
    Messenger(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_ = nullptr,
        std::string delim_ = "\r\n");

    int revents();

    bool did_timeout();
    void set_timeout(int ms);
    int get_timeout();
    void reset_timeout();
    int get_fd();
    bool closed();
    void unset_flags(short  mask);
    void reset_flags();

    virtual ~Messenger();
};

class MessengerIN : virtual public Messenger {
  public:
    static constexpr int buffor_size = 1000;
    static constexpr int readlen = 100;

  protected:
    std::vector<char> buffor;
    size_t size = 0;

  public:
    MessengerIN(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_ = nullptr,
        std::string delim_ = "\r\n");

    void read();

    std::vector<std::string> get_msgs();

    std::vector<std::string> runIN();
    
    virtual ~MessengerIN();
};

class MessengerOUT : virtual public Messenger {
  protected:
    std::queue<std::string> buffor_to_send;
    size_t sended = 0;
  public:
    MessengerOUT(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_ = nullptr,
        std::string delim_ = "\r\n");

    bool write();

    void send_msg(const std::string &msg);

    void send_msgs(const std::vector<std::string> &msgs);
    void send_if_no_timeout(const std::string &msg, int ms);

    int send_size();

    void runOUT();

    virtual ~MessengerOUT() = default;
};

class MessengerBI : public MessengerIN, public MessengerOUT {
  public:
    MessengerBI(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_ = nullptr,
        std::string delim_ = "\r\n");

    std::vector<std::string> run();
};

class Reporter : public MessengerOUT {
  private:
    // Place for local address.

    std::string get_time();

  public:
    Reporter(int desc_, Poller &poller_);

    void transmission(std::string msg, std::string from, std::string to);

    void sendedTo(const std::string &me, const std::string &peer, 
                  const std::string &msg);

    void recvdFrom(const std::string &me, const std::string &peer, 
                  const std::string &msg);
};


inline Poller::Poller() {}

inline struct pollfd& Poller::get(std::size_t i) { return poll_descriptors[i]; }

inline void Poller::set_timeout(std::size_t i, int ms) { 
    timeouts[i] = ms; 
    did_timeout_[i] = false; 
}

inline int Poller::get_timeout(std::size_t i) { return timeouts[i]; }
inline bool Poller::did_timeout(std::size_t i) { return did_timeout_[i]; }

inline uint Poller::size() {
    uint ret = 0;
    for (const auto& e : poll_descriptors)
        if (e.fd != INVALID_FD)
            ret++;
    return ret;
}

inline std::size_t Poller::add(int fd) {
    // Reuse old index if possible.
    for (std::size_t i = 0; i < poll_descriptors.size(); i++) {
        if (poll_descriptors[i].fd == INVALID_FD) {
            poll_descriptors[i].fd = fd;
            debuglog << "POLL: Adding new fd " << fd 
                     << " on index " << i << "\n";
            return i;
        }
    }

    // Create new index.
    poll_descriptors.push_back({.fd = fd, .events = 0, .revents = 0});
    timeouts.push_back(0);
    did_timeout_.push_back(false);

    debuglog << "POLL: Adding new fd " << fd << " on index " 
                << poll_descriptors.size() - 1 << "\n";

    return poll_descriptors.size() - 1;
}

inline void Poller::rm(std::size_t i) {
    debuglog << "POLL: removing index " << i << "\n";
    poll_descriptors[i] = {.fd = INVALID_FD, .events = 0, .revents = 0};
    timeouts[i] = 0;
}

inline int Poller::run() {
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
        did_timeout_[i] = false;
        poll_descriptors[i].revents = 0;
    }

    auto start = std::chrono::steady_clock::now();
    int poll_status = poll(poll_descriptors.data(), 
                            poll_descriptors.size(), timeout);

    int duration = 
        (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();

    // Check if descriptors timed out.
    for (std::size_t i = 0; i < timeouts.size(); i++) {
        if (timeouts[i] == 0)
            continue;
        timeouts[i] = std::max(0, timeouts[i] - duration);
        did_timeout_[i] = (timeouts[i] == 0);
    }

    debuglog << "POLL: POLLSTTATUS " << poll_status 
             << " TIMEOUT " << timeout << "\n";
    print_debug();

    return poll_status;
}

inline void Poller::print_debug() {
    debuglog << "POLL: READ WRITE ERR " << POLLIN << " " << POLLOUT << " " 
                                        << POLLERR << "\n";
    debuglog << "POLL: fd events revents did_timeout\n";
    for (std::size_t i = 0; i < timeouts.size(); i++) {
        if (poll_descriptors[i].fd != INVALID_FD) {
            debuglog << "POLL: " 
                     << poll_descriptors[i].fd << " "
                     << poll_descriptors[i].events << " "
                     << poll_descriptors[i].revents << " "
                     << did_timeout_[i] << "\n";
        }
    }
}

inline Messenger::Messenger(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_,
        std::string delim_) : desc(desc_), poller(poller_), logger(logger_), 
    my_name(my_name_), peer_name(peer_name_), delim(delim_) {
    poll_idx = poller.add(desc);
    poller.get(poll_idx).events = 0;
}

inline int Messenger::revents() { return poller.get(poll_idx).revents; } 

inline bool Messenger::did_timeout() { return poller.did_timeout(poll_idx); }
inline void Messenger::set_timeout(int ms) { poller.set_timeout(poll_idx, ms); }
inline int Messenger::get_timeout() { return poller.get_timeout(poll_idx); }
inline void Messenger::reset_timeout() { return set_timeout(0); }
inline int Messenger::get_fd() { return desc; }
inline bool Messenger::closed() {return is_closed; }

inline void Messenger::unset_flags(short int mask) {
    last_flags = poller.get(poll_idx).events;
    poller.get(poll_idx).events &= (~mask);
}

inline void Messenger::reset_flags() {
    poller.get(poll_idx).events = last_flags;
}

inline Messenger::~Messenger() {
    poller.rm(poll_idx);
    close(desc);
}


inline MessengerIN::MessengerIN(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_,
        std::string delim_) : 
    Messenger(desc_, poller_, my_name_, peer_name_, logger_, delim_), 
    buffor(buffor_size) {
    poller.get(poll_idx).events |= POLLIN;
    last_flags |= POLLIN;
}

inline void MessengerIN::read() {
    ssize_t len_read = ::read(desc, buffor.data() + size, readlen);

    if (len_read < 0) {
        is_closed = true;
        throw syscall_error("read", len_read);
    } else if (len_read == 0) { 
        is_closed = true;
        return;
    }
    size += len_read;
}

inline std::vector<std::string> MessengerIN::get_msgs() {
    read();

    std::vector<std::string> msgs;

    while (true) {
        auto sep = std::ranges::search(
            buffor.begin() , buffor.begin() + size,
            delim.begin(), delim.end());

        if (sep.empty()) { break; }

        msgs.emplace_back(buffor.begin(), sep.begin());
        if (logger)
            logger->recvdFrom(my_name, peer_name,
                msgs.back() + special_printer(delim));

        size -= std::distance(buffor.begin(), sep.end());
        buffor.erase(buffor.begin(), sep.end());
    }

    buffor.resize(buffor_size);
    if (size > buffor_size - readlen) {
        if (logger)
            logger->recvdFrom(my_name, peer_name, 
                std::string(buffor.begin(), buffor.begin() + size) + 
                special_printer(delim));
        throw std::overflow_error("Buffor on desc " + std::to_string(desc) + 
            "about to overflow. Incoming messege to long");
    }

    for (auto &e : msgs) {
        debuglog << "MSNGER rcvd: " << std::quoted(e) << "\n";
    }

    return msgs;
}

inline std::vector<std::string> MessengerIN::runIN() {
    if (revents() & (POLLERR | POLLPRI | POLLRDHUP | POLLHUP | POLLNVAL)) {
        is_closed = true;
    }

    if (revents() & (POLLIN)) {
        return get_msgs();
    }

    return {};
}

inline MessengerIN::~MessengerIN() {
    if (logger && size > 0)
        logger->recvdFrom(my_name, peer_name, 
                std::string(buffor.begin(), buffor.begin() + size) + 
                special_printer(delim));
}

inline MessengerOUT::MessengerOUT(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_,
        std::string delim_) :
        Messenger(desc_, poller_, my_name_, peer_name_, logger_, delim_) {}

inline bool MessengerOUT::write() {
    if (buffor_to_send.empty()) {
        return false;
    }

    ssize_t len_write = 
        ::write(desc, buffor_to_send.front().data() + sended, 
            buffor_to_send.front().size() - sended);

    if (len_write < 0) {
        is_closed = true;
        throw syscall_error("write", len_write);
    } else if (len_write == 0) { 
        is_closed = true;
        throw syscall_error("write == 0", len_write);
    }

    sended += len_write;
    if (sended == buffor_to_send.front().size()) {
        if (logger)
            logger->sendedTo(my_name, peer_name, 
                buffor_to_send.front());
        buffor_to_send.pop();
        sended = 0;
        if (buffor_to_send.empty())
            poller.get(poll_idx).events &= (~POLLOUT);

        return true;
    }

    return false;
}

inline void MessengerOUT::send_msg(const std::string &msg) {
    poller.get(poll_idx).events |= POLLOUT;
    buffor_to_send.push(msg + delim);
}

inline void MessengerOUT::send_msgs(const std::vector<std::string> &msgs) {
    for (const auto &e : msgs) send_msg(e);
}

inline void MessengerOUT::send_if_no_timeout(const std::string &msg, int ms) {
    if (poller.get_timeout(poll_idx) == 0) {
        send_msg(msg);
        set_timeout(ms);
    }
}

inline int MessengerOUT::send_size() { return (int)buffor_to_send.size(); }

inline void MessengerOUT::runOUT() { 
    if (revents() & (POLLERR | POLLPRI | POLLRDHUP | POLLHUP | POLLNVAL)) {
        is_closed = true;
    }

    if (revents() & POLLOUT) write(); 
}

inline MessengerBI::MessengerBI(int desc_, Poller &poller_, std::string my_name_, 
        std::string peer_name_, std::shared_ptr<Reporter> logger_,
        std::string delim_) : 
    Messenger(desc_, poller_, my_name_, peer_name_, logger_, delim_),
    MessengerIN(desc_, poller_, my_name_, peer_name_, logger_, delim_),
    MessengerOUT(desc_, poller_, my_name_, peer_name_, logger_, delim_) {}

inline std::vector<std::string> MessengerBI::run() {
    // Every option except for POLLIN and POLLOUT.
    if (revents() & (POLLERR | POLLPRI | POLLRDHUP | POLLHUP | POLLNVAL)) {
        is_closed = true;
    }

    runOUT();
    return runIN();
}

inline Reporter::Reporter(int desc_, Poller &poller_) : 
        Messenger(desc_, poller_, "", "", nullptr, ""), 
        MessengerOUT(desc_, poller_, "", "", nullptr, "") {}

inline std::string Reporter::get_time() {
    // std::format is in g++ from version 13.
    // Until then we need to use put_time and stringstreams
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto milis = std::chrono::duration_cast<std::chrono::milliseconds>
                                (now.time_since_epoch()).count() % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%dT%H:%M:%S.")
        << std::setfill('0') << std::setw(3) << milis;
    return ss.str();
}

inline void Reporter::transmission(std::string msg, std::string from, std::string to) {
    send_msg("[" +  from + "," + to + "," + get_time() + "] " + msg);
}

inline void Reporter::sendedTo(const std::string &me, const std::string &peer, 
                        const std::string &msg) {
    transmission(msg, me, peer);
}

inline void Reporter::recvdFrom(const std::string &me, const std::string &peer, 
                        const std::string &msg) {
    transmission(msg, peer, me);
}

#endif /* INOUT_HPP */