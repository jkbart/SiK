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

#include <cstdlib>
#include <random>

#include "io.hpp"
#include "protconst.h"

namespace ASIO {
enum protocol_t : int8_t { tcp = 1, udp = 2, udpr = 3 };

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

    const char *what() { return _msg.c_str(); }
};

int64_t session_id_generate() {
    static std::mt19937_64 gen(std::time(0));
    return gen();
}

class PacketBase {
  public:
    const int64_t _session_id;

  protected:
    PacketBase(int64_t session_id) : _session_id(session_id) {}

  public:
    virtual void send(IO::Socket &socket, sockaddr_in *receiver) = 0;
    virtual packet_type_t getID() = 0;
};

template <packet_type_t P> class Packet;

template <> class Packet<CONN> : public PacketBase {
  public:
    static const packet_type_t _id = CONN;
    const protocol_t _protocol;
    const int64_t _data_len;

  public:
    Packet<CONN>(int64_t session_id, protocol_t protocol, int64_t data_len)
        : PacketBase(session_id), _protocol(protocol), _data_len(data_len) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::send_v(socket, receiver, _id, _session_id, _protocol, _data_len);
    }

    packet_type_t getID() { return _id; }

    static Packet<CONN> read(IO::PacketReaderBase &reader) {
        auto [session_id, protocol, data_len] =
            reader.readGeneric<int64_t, protocol_t, int64_t>();
        return Packet<CONN>(session_id, protocol, data_len);
    }

    static Packet<CONN> read(IO::PacketReaderBase &reader, int64_t session_id) {
        auto [protocol, data_len] = reader.readGeneric<protocol_t, int64_t>();
        return Packet<CONN>(session_id, protocol, data_len);
    }
};

template <> class Packet<CONNACC> : public PacketBase {
  public:
    static const packet_type_t _id = CONNACC;

  public:
    Packet<CONNACC>(int64_t session_id) : PacketBase(session_id) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::send_v(socket, receiver, _id, _session_id);
    }

    packet_type_t getID() { return _id; }

    static Packet<CONNACC> read(IO::PacketReaderBase &reader) {
        auto [session_id] = reader.readGeneric<int64_t>();
        return Packet<CONNACC>(session_id);
    }

    static Packet<CONNACC> read(IO::PacketReaderBase &reader,
                                int64_t session_id) {
        return Packet<CONNACC>(session_id);
    }
};

template <> class Packet<CONNRJT> : public PacketBase {
  public:
    static const packet_type_t _id = CONNRJT;

  public:
    Packet<CONNRJT>(int64_t session_id) : PacketBase(session_id) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::send_v(socket, receiver, _id, _session_id);
    }

    packet_type_t getID() { return _id; }

    static Packet<CONNRJT> read(IO::PacketReaderBase &reader) {
        auto [session_id] = reader.readGeneric<int64_t>();
        return Packet<CONNRJT>(session_id);
    }

    static Packet<CONNRJT> read(IO::PacketReaderBase &reader,
                                int64_t session_id) {
        return Packet<CONNRJT>(session_id);
    }
};

template <> class Packet<DATA> : public PacketBase {
  public:
    static const packet_type_t _id = DATA;
    const int64_t _packet_number;
    const int32_t _packet_byte_cnt;
    const std::vector<int8_t> _data;

  public:
    Packet<DATA>(int64_t session_id, int64_t packet_number,
                 int32_t packet_byte_cnt, void *data)
        : PacketBase(session_id), _packet_number(packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(packet_byte_cnt) {
        std::memcpy((void *)_data.data(), data, packet_byte_cnt);
    }
    Packet<DATA>(int64_t session_id, int64_t packet_number,
                 int32_t packet_byte_cnt, std::vector<int8_t> data)
        : PacketBase(session_id), _packet_number(packet_number),
          _packet_byte_cnt(packet_byte_cnt), _data(data) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        // send_v<packet_type_t, int64_t, protocol_t, int64_t> (socket,
        // receiver, _id,  _session_id, _protocol, _data_len); send_v(socket,
        // receiver, _id, _session_id);
        IO::PacketSender sender(socket, receiver);
        sender.add_var(_id, _packet_number, _packet_byte_cnt);
        sender.add_data((char *)_data.data(), _data.size());
        sender.send();
    }

    packet_type_t getID() { return _id; }

    static Packet<DATA> read(IO::PacketReaderBase &reader) {
        auto [session_id, packet_number, packet_byte_cnt] =
            reader.readGeneric<int64_t, int64_t, int32_t>();

        return Packet<DATA>(session_id, packet_number, packet_byte_cnt,
                            reader.readn(packet_byte_cnt));
    }

    static Packet<DATA> read(IO::PacketReaderBase &reader, int64_t session_id) {
        auto [packet_number, packet_byte_cnt] =
            reader.readGeneric<int64_t, int32_t>();

        return Packet<DATA>(session_id, packet_number, packet_byte_cnt,
                            reader.readn(packet_byte_cnt));
    }
};

template <> class Packet<ACC> : public PacketBase {
  public:
    static const packet_type_t _id = ACC;
    const int64_t _packet_number;

  public:
    Packet<ACC>(int64_t session_id, int64_t packet_number)
        : PacketBase(session_id), _packet_number(packet_number) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::send_v(socket, receiver, _id, _session_id, _packet_number);
    }

    packet_type_t getID() { return _id; }

    static Packet<ACC> read(IO::PacketReaderBase &reader) {
        auto [session_id, packet_number] =
            reader.readGeneric<int64_t, int64_t>();
        return Packet<ACC>(session_id, packet_number);
    }

    static Packet<ACC> read(IO::PacketReaderBase &reader, int64_t session_id) {
        auto [packet_number] = reader.readGeneric<int64_t>();
        return Packet<ACC>(session_id, packet_number);
    }
};

template <> class Packet<RJT> : public PacketBase {
  public:
    static const packet_type_t _id = RJT;
    const int64_t _packet_number;

  public:
    Packet<RJT>(int64_t session_id, int64_t packet_number)
        : PacketBase(session_id), _packet_number(packet_number) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        IO::send_v(socket, receiver, _id, _session_id, _packet_number);
    }

    packet_type_t getID() { return _id; }

    static Packet<RJT> read(IO::PacketReaderBase &reader) {
        auto [session_id, packet_number] =
            reader.readGeneric<int64_t, int64_t>();
        return Packet<RJT>(session_id, packet_number);
    }

    static Packet<RJT> read(IO::PacketReaderBase &reader, int64_t session_id) {
        auto [packet_number] = reader.readGeneric<int64_t>();
        return Packet<RJT>(session_id, packet_number);
    }
};

template <> class Packet<RCVD> : public PacketBase {
  public:
    static const packet_type_t _id = RCVD;

  public:
    Packet<RCVD>(int64_t session_id) : PacketBase(session_id) {}

    void send(IO::Socket &socket, sockaddr_in *receiver) {
        // send_v<packet_type_t, int64_t, protocol_t, int64_t> (socket,
        // receiver, _id,  _session_id, _protocol, _data_len);
        IO::send_v(socket, receiver, _id, _session_id);
    }

    packet_type_t getID() { return _id; }

    static Packet<RCVD> read(IO::PacketReaderBase &reader) {
        auto [session_id] = reader.readGeneric<int64_t>();
        return Packet<RCVD>(session_id);
    }

    static Packet<RCVD> read(IO::PacketReaderBase &reader, int64_t session_id) {
        return Packet<RCVD>(session_id);
    }
};

template <IO::Socket::connection_t C>
std::tuple<IO::PacketReader<C> &, packet_type_t>
get_next_from_session(IO::Socket &socket, sockaddr_in client_address,
                      int64_t current_session_id);

template <>
std::tuple<IO::PacketReader<IO::Socket::UDP> &, packet_type_t>
get_next_from_session<IO::Socket::UDP>(IO::Socket &socket,
                                       sockaddr_in client_address,
                                       int64_t current_session_id) {
    auto begin = std::chrono::steady_clock::now();

    while (true) {
        sockaddr_in addr;
        IO::PacketReader<IO::Socket::UDP> reader(socket, &addr, true, begin);
        auto [id, session_id] = reader.readGeneric<packet_type_t, int64_t>();

        if (session_id == current_session_id && addr == client_address) {
            return {reader, id};
        } else if (id == CONN) {
            Packet<CONNRJT>(session_id).send(socket, &addr);
        }
    }
}

template <>
std::tuple<IO::PacketReader<IO::Socket::TCP> &, packet_type_t>
get_next_from_session<IO::Socket::TCP>(IO::Socket &socket,
                                       sockaddr_in client_address,
                                       int64_t current_session_id) {

    IO::PacketReader<IO::Socket::TCP> reader(socket, NULL);
    auto [id, session_id] = reader.readGeneric<packet_type_t, int64_t>();

    if (session_id != current_session_id) {
        throw std::runtime_error(
            std::string("Received unexptected session_id: ") +
            std::string("expected: ") + std::to_string(current_session_id) +
            std::string("received: ") + std::to_string(session_id));
    }

    return {reader, id};
}
} // namespace ASIO

#endif /* COMMON_HPP */