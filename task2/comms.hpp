#ifndef COMMS_HPP
#define COMMS_HPP

#include "debug.hpp"
#include "common.hpp"
#include "deals.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <exception>
#include <tuple>
#include <algorithm>

class COMMS {
  protected:
    const std::string _msg;
    COMMS(std::string msg) : _msg(msg) {} // Making class abstract.
  public:
    void send() {
        // send _msg;
    }

    virtual ~COMMS();
};

class IAM : public COMMS {
  public:
    inline const static std::string _prefix = "IAM";
    const Place _place;

  private:
    Place get_place(std::string_view &&to_parse) {
        if (!to_parse.starts_with(_prefix)) {
            // TODO: better excpetion
            throw std::runtime_error("IAM NOT CORRECT FORMAT");
        }

        to_parse.remove_prefix(_prefix.size());

        Place ret(to_parse);

        if (!to_parse.empty()) {
            throw std::runtime_error("IAM NOT CORRECT FORMAT");
        }

        return ret;
    }

  public:
    IAM(const std::string &msg) : 
    COMMS(msg), _place(get_place(std::string_view(_msg))) {}

    IAM(Place place) : 
    COMMS(_prefix + (std::string)place), _place(place) {}
};

class BUSY : public COMMS {
  public:
    inline const static std::string _prefix = "BUSY";
    const std::vector<Place> _list;

  private:
    std::vector<Place> get_places(std::string_view &&to_parse) {
        if (!to_parse.starts_with(_prefix)) {
            // TODO: better excpetion
            throw std::runtime_error("BUSY NOT CORRECT FORMAT");
        }

        to_parse.remove_prefix(_prefix.size());

        std::vector<Place> ret;

        while (!to_parse.empty()) {
            Place last(to_parse);
            if (std::find(ret.begin(), ret.end(), last) != ret.end()) {
                // TODO: better excpetion
                throw std::runtime_error("BUSY NOT CORRECT FORMAT");
            }

            ret.push_back(last);
        }

        return ret;
    }

    std::string list_to_string(std::vector<Place> list) {
        std::string ans = "";
        for (auto e : list) {
            ans += e;
        }
        return ans;
    }

  public:
    BUSY(const std::string &msg) : 
    COMMS(msg), _list(get_places(std::string_view(_msg))) {}

    BUSY(std::vector<Place> list) : 
    COMMS(_prefix + list_to_string(list)), _list(list) {}
};

#endif
