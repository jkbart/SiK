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

#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <tuple>

#include <iostream>

#include <cstdlib>
#include <random>

#include "io.hpp"
#include "protconst.h"

namespace ASIO {
enum protocol_t : int8_t { tcp = 1, udp = 2, udpr = 3 };

using p_cnt_t = uint32_t;
using b_cnt_t = uint64_t;
using session_t = uint64_t;

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
    packet_type_t _received;
    std::string _msg;

  public:
    unexpected_packet(packet_type_t expected, packet_type_t received)
        : _expected(expected), _received(received),
          _msg("Unexptected packet: expected:" + packet_to_string(_expected) +
               ", received:" + packet_to_string(_received)) {}

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

session_t session_id_generate() {
    // using std::time(0) is not good enough
    static std::mt19937_64 gen(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
    return gen();
}

class PacketBase {
  public:
    const session_t _session_id;

  public:
    PacketBase(session_t session_id) : _session_id(session_id) {}
    PacketBase(IO::PacketReaderBase &reader) 
    : _session_id(std::get<1>(reader.readGeneric<packet_type_t, session_t>())) {}

    void send(IO::PacketSender &sender) {
        sender.add_var<packet_type_t, session_t>(getID(), _session_id);
    }

  public:
    virtual void send(IO::Socket &socket, sockaddr_in *receiver) = 0;
    virtual packet_type_t getID() = 0;
};

class PacketOrderedBase : public PacketBase {
  public:
    const p_cnt_t _packet_number;

  public:
    PacketOrderedBase(session_t session_id, p_cnt_t packet_number) 
    : PacketBase(session_id), _packet_number(packet_number) {}


    PacketOrderedBase(IO::PacketReaderBase &reader) 
    : PacketBase(reader), 
    _packet_number(std::get<0>(reader.readGeneric<p_cnt_t>())) {}


    void send(IO::PacketSender &sender) {
        PacketBase::send(sender);
        sender.add_var<p_cnt_t>(_packet_number);
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
    _data_len(std::get<0>(reader.readGeneric<b_cnt_t>())) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     IO::send_v(socket, receiver, _id, _session_id, _protocol, _data_len);
    // }

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.add_var<protocol_t, b_cnt_t>(_protocol, _data_len);
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<CONN> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id, protocol, data_len] =
            reader.readGeneric<session_t, protocol_t, b_cnt_t>();
        return Packet<CONN>(session_id, protocol, data_len);
    }
};

template <> class Packet<CONNACC> : public PacketBase {
  public:
    static const packet_type_t _id = CONNACC;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}
    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     IO::send_v(socket, receiver, _id, _session_id);
    // }
    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<CONNACC> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id] = reader.readGeneric<session_t>();
        return Packet<CONNACC>(session_id);
    }
};

template <> class Packet<CONNRJT> : public PacketBase {
  public:
    static const packet_type_t _id = CONNRJT;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}
    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     IO::send_v(socket, receiver, _id, _session_id);
    // }

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        // sender.add_var<protocol_t>(_protocol);
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<CONNRJT> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id] = reader.readGeneric<session_t>();
        return Packet<CONNRJT>(session_id);
    }
};

template <> class Packet<DATA> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = DATA;
    // const uint64_t _packet_number;
    const b_cnt_t _packet_byte_cnt;
    const std::vector<char> _data;

  public:
    Packet(session_t session_id, p_cnt_t packet_number,
                 b_cnt_t packet_byte_cnt, void *data)
        : PacketOrderedBase(session_id, packet_number), // _packet_number(packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(packet_byte_cnt) {
        std::memcpy((void *)_data.data(), data, packet_byte_cnt);
    }
    Packet(session_t session_id, p_cnt_t packet_number,
                 b_cnt_t packet_byte_cnt, std::vector<char> data)
        : PacketOrderedBase(session_id, packet_number), // _packet_number(packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(data) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader), 
    _packet_byte_cnt(std::get<0>(reader.readGeneric<b_cnt_t>())),
    _data(reader.readn(_packet_byte_cnt)) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     // send_v<packet_type_t, uint64_t, protocol_t, uint64_t> (socket,
    //     // receiver, _id,  _session_id, _protocol, _data_len); send_v(socket,
    //     // receiver, _id, _session_id);
    //     IO::PacketSender sender(socket, receiver);
    //     sender.add_var(_id, _session_id, _packet_number, _packet_byte_cnt);
    //     sender.add_data((char *)_data.data(), _data.size());
    //     sender.send();
    // }
    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        sender.add_var<b_cnt_t>(_packet_byte_cnt);
        sender.add_data((char *)_data.data(), _data.size());
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<DATA> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id, packet_number, packet_byte_cnt] =
            reader.readGeneric<session_t, p_cnt_t, b_cnt_t>();

        return Packet<DATA>(session_id, packet_number, packet_byte_cnt,
                            reader.readn(packet_byte_cnt));
    }
};

template <> class Packet<ACC> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = ACC;
    // const uint64_t _packet_number;

  public:
    Packet(session_t session_id, p_cnt_t packet_number)
        : PacketOrderedBase(session_id, packet_number) {} //, _packet_number(packet_number) {}
        
    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     IO::send_v(socket, receiver, _id, _session_id, _packet_number);
    // }
    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        // sender.add_var<protocol_t>(_protocol);
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<ACC> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id, packet_number] =
            reader.readGeneric<session_t, p_cnt_t>();
        return Packet<ACC>(session_id, packet_number);
    }
};

template <> class Packet<RJT> : public PacketOrderedBase {
  public:
    static const packet_type_t _id = RJT;
    // const uint64_t _packet_number;

  public:
    Packet(session_t session_id, p_cnt_t packet_number)
        : PacketOrderedBase(session_id, packet_number) {} //, _packet_number(packet_number) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketOrderedBase(reader) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     IO::send_v(socket, receiver, _id, _session_id, _packet_number);
    // }
    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketOrderedBase::send(sender);
        // sender.add_var<protocol_t>(_protocol);
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<RJT> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id, packet_number] =
            reader.readGeneric<session_t, p_cnt_t>();
        return Packet<RJT>(session_id, packet_number);
    }
};

template <> class Packet<RCVD> : public PacketBase {
  public:
    static const packet_type_t _id = RCVD;

  public:
    Packet(session_t session_id) : PacketBase(session_id) {}

    Packet(IO::PacketReaderBase &reader) 
    : PacketBase(reader) {}

    // void send(IO::Socket &socket, sockaddr_in *receiver) {
    //     // send_v<packet_type_t, uint64_t, protocol_t, uint64_t> (socket,
    //     // receiver, _id,  _session_id, _protocol, _data_len);
    //     IO::send_v(socket, receiver, _id, _session_id);
    // }
    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::PacketSender sender(socket, receiver);
        PacketBase::send(sender);
        sender.send();
    }


    packet_type_t getID() { return _id; }

    static Packet<RCVD> read(IO::PacketReaderBase &reader) {
        reader.mtb();
        auto [session_id] = reader.readGeneric<session_t>();
        return Packet<RCVD>(session_id);
    }
};

} // namespace ASIO

#endif /* COMMON_HPP */