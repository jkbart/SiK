#ifndef COMMON_HPP
#define COMMON_HPP

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <endian.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <optional>

#include <iostream>

#include <cstdlib>
#include <random>

#include "io.hpp"
#include "protconst.h"

namespace PPCB {
enum protocol_t : int8_t { tcp = 1, udp = 2, udpr = 3 };

// p_cnt_t:   packet number type.
// b_cnt_t:   byte count type.
// session_t: type used for session id.
using p_cnt_t = uint32_t;
using b_cnt_t = uint64_t;
using session_t = uint64_t;

template<class T>
T to_host(T v);

template<class T>
T to_net(T v);

template<>
p_cnt_t to_host<p_cnt_t>(p_cnt_t v) {
    return be32toh(v);
}

template<>
b_cnt_t to_host<b_cnt_t>(b_cnt_t v) {
    return be64toh(v);
}

template<>
p_cnt_t to_net<p_cnt_t>(p_cnt_t v) {
    return htobe32(v);
}

template<>
b_cnt_t to_net<b_cnt_t>(b_cnt_t v) {
    return htobe64(v);
}

enum packet_type_t : int8_t {
    CONN = 1,
    CONNACC = 2,
    CONNRJT = 3,
    DATA = 4,
    ACC = 5,
    RJT = 6,
    RCVD = 7
};

std::string packet_to_string(packet_type_t packet_type) {
    switch (packet_type) {
    case CONN:
        return "CONN";
    case CONNACC:
        return "CONNACC";
    case CONNRJT:
        return "CONNRJT";
    case DATA:
        return "DATA";
    case ACC:
        return "ACC";
    case RJT:
        return "RJT";
    case RCVD:
        return "RCVD";
    default:
        return "Unkown packet type";
    }
}

class unexpected_packet : public std::exception {
  private:
    packet_type_t _expected;
    std::optional<int> _e_nr;
    packet_type_t _received;
    std::optional<int> _r_nr;
    std::string _msg;

  public:
    unexpected_packet(packet_type_t expected, std::optional<int> e_nr, 
    packet_type_t received, std::optional<int> r_nr)
    : _expected(expected), _e_nr(e_nr),
    _received(received), _r_nr(r_nr) {
        std::string packet_expected, packet_received;
        if (_e_nr) {
            packet_expected = "<" + packet_to_string(_expected) + 
                " nr:" + std::to_string(_e_nr.value())  + ">"; 
        } else {
            packet_expected = "<" + packet_to_string(_expected) + ">"; 
        }

        if (_r_nr) {
            packet_received = "<" + packet_to_string(_received) + 
                " nr:" + std::to_string(_r_nr.value())  + ">"; 
        } else {
            packet_received = "<" + packet_to_string(_received) + ">"; 
        }
        _msg = "Unexptected packet: expected: " + packet_expected + 
            ", received: " + packet_received;
    }

    const char *what() const throw() { return _msg.c_str(); }
};

class rejected_data : public std::exception {
  private:
    std::string _msg;
    p_cnt_t _packet_number;
  public:
    rejected_data(p_cnt_t packet_number) : _packet_number(packet_number),
          _msg("Data packet " + std::to_string(packet_number) + " rejected") {}

    const char *what() const throw() { return _msg.c_str(); }
};

// Random session id generator.
session_t session_id_generate() {
    static std::mt19937_64 gen(std::random_device{}());
    return gen();
}

// Packet types classes.
class PacketBase {
  public:
    const session_t _session_id;

  public:
    PacketBase(session_t session_id) : _session_id(session_id) {}
    PacketBase(IO::PacketReaderBase &reader) 
    : _session_id(std::get<1>(reader.readGeneric<packet_type_t, session_t>())) 
    {}

    void send(IO::PacketSender &sender) const {
        sender.add_var<packet_type_t, session_t>(getID(), _session_id);
    }

    virtual void printer(std::ostream& os) const {
        os << "<" << packet_to_string(getID()) << ">";
    }

    friend std::ostream& operator<<(std::ostream& os, PacketBase const& a) {
        a.printer(os);
        return os;
    }

    virtual void send(IO::Socket &socket, sockaddr_in *receiver) const = 0;
    virtual packet_type_t getID() const = 0;
};

class PacketOrderedBase : public PacketBase {
  public:
    const p_cnt_t _packet_number;

  public:
    PacketOrderedBase(session_t session_id, p_cnt_t packet_number) 
    : PacketBase(session_id), _packet_number(packet_number) {}


    PacketOrderedBase(IO::PacketReaderBase &reader) 
    : PacketBase(reader), 
    _packet_number(to_host(std::get<0>(reader.readGeneric<p_cnt_t>()))) {}


    void send(IO::PacketSender &sender) const {
        PacketBase::send(sender);
        sender.add_var<p_cnt_t>(to_net(_packet_number));
    }

    virtual packet_type_t getID() const = 0;
    virtual void printer(std::ostream& os) const {
        os << "<" << packet_to_string(getID()) << " nr:" << _packet_number << ">";
    }
};

template <packet_type_t P> class Packet;

template <> class Packet<CONN> : public PacketBase {
  public:
    static const packet_type_t _id = CONN;
    const protocol_t _protocol;
    const b_cnt_t _data_len;

  public:
    Packet(session_t session_id, protocol_t protocol, b_cnt_t data_len)
        : PacketBase(session_id), _protocol(protocol), _data_len(data_len) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader), 
    _protocol(std::get<0>(reader.readGeneric<protocol_t>())),
    _data_len(to_host(std::get<0>(reader.readGeneric<b_cnt_t>()))) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.add_var<protocol_t, b_cnt_t>(_protocol, to_net(_data_len));
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<CONNACC> : public PacketBase {
  public:
    static const packet_type_t _id = CONNACC;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}
    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<CONNRJT> : public PacketBase {
  public:
    static const packet_type_t _id = CONNRJT;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}
    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<DATA> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = DATA;
    const b_cnt_t _packet_byte_cnt;
    const std::vector<char> _data;

  public:
    Packet(session_t session_id, p_cnt_t packet_number,
                 b_cnt_t packet_byte_cnt, void *data)
        : PacketOrderedBase(session_id, packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(packet_byte_cnt) {
        std::memcpy((void *)_data.data(), data, packet_byte_cnt);
    }
    Packet(session_t session_id, p_cnt_t packet_number,
                 b_cnt_t packet_byte_cnt, std::vector<char> data)
        : PacketOrderedBase(session_id, packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(data) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader), 
    _packet_byte_cnt(to_host(std::get<0>(reader.readGeneric<b_cnt_t>()))),
    _data(reader.readn(_packet_byte_cnt)) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        sender.add_var<b_cnt_t>(to_net(_packet_byte_cnt));
        sender.add_data((char *)_data.data(), _data.size());
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<ACC> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = ACC;

  public:
    Packet(session_t session_id, p_cnt_t packet_number)
        : PacketOrderedBase(session_id, packet_number) {}
        
    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<RJT> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = RJT;

  public:
    Packet(session_t session_id, p_cnt_t packet_number)
        : PacketOrderedBase(session_id, packet_number) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        sender.send();
    }

    packet_type_t getID() const { return _id; }
};

template <> class Packet<RCVD> : public PacketBase {
  public:
    static const packet_type_t _id = RCVD;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) const {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.send();
    }


    packet_type_t getID() const { return _id; }
};

} // namespace PPCB

#endif /* COMMON_HPP */