#ifndef __TILELINKLIB_HPP__
#define __TILELINKLIB_HPP__

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <cstdint>
#include <string>

class TLAgent {
public:
  TLAgent() {}

protected:
  TLBundleParams p;
};

// Client Agent class (Master/Source)
class ClientTLAgent : public TLAgent {
public:
  ClientTLAgent(const std::string &uri, size_t capacity = 0, bool fresh = false,
              double max_rate = -1) {

    a_tx.init(uri + "_a.q", capacity, fresh, max_rate);
    d_rx.init(uri + "_d.q", capacity, fresh, max_rate);
    p = {.address_bit_width = 64,
         .source_bit_width = 32,
         .sink_bit_width = 32,
         .size_bit_width = 4,
         .data_bit_width = 256,
        };
  }

  void send_a(const TLMessageA &tlA) {
    sb_packet packet;
    memcpy(&packet.data, &tlA, sizeof(tlA));
    a_tx.send_blocking(packet);
  }

  void recv_d(TLMessageD &tlD) {
    sb_packet packet;
    d_rx.recv_blocking(packet);
    memcpy(&tlD, &packet.data, sizeof(tlD));
  }

private:
  SBTX a_tx; // A channel
  SBRX d_rx; // D channel
};

// Manager Agent class (Slave/Sink)
class ManagerTLAgent : public TLAgent {
public:
  ManagerTLAgent(const std::string &uri, size_t capacity = 0, bool fresh = false,
               double max_rate = -1) {
    a_rx.init(uri + "_a.q", capacity, fresh, max_rate);
    d_tx.init(uri + "_d.q", capacity, fresh, max_rate);
    p = {.address_bit_width = 64,
         .source_bit_width = 32,
         .sink_bit_width = 32,
         .size_bit_width = 4,
         .data_bit_width = 256};
  }

  void recv_a(TLMessageA &tlA) {
    sb_packet packet;
    a_rx.recv_blocking(packet);
    memcpy(&tlA, &packet.data, sizeof(tlA));
  }

  void send_d(const TLMessageD &tlD) {
    sb_packet packet;
    memcpy(&packet.data, &tlD, sizeof(tlD));
    d_tx.send_blocking(packet);
  }

private:
  SBRX a_rx; // A channel
  SBTX d_tx; // D channel
};

static inline std::string tlA_to_str(TLMessageA p,
                                     const TLBundleParams &params) {
  /*
      // determine how many bytes to print
      size_t max_idx;
      if (nbytes < 0) {
          max_idx = sizeof(p.data);
      } else {
          max_idx = nbytes;
      }

      // used for convenient formatting with sprintf
      char buf[128];

      // build up return value
      std::string retval;
      retval = "";

      // format control information
      sprintf(buf, "dest: %08x, last: %d, data: {", p.destination, p.last);
      retval += buf;

      // format data
      for (size_t i = 0; i < max_idx; i++) {
          sprintf(buf, "%02x", p.data[i]);
          retval += buf;
          if (i != (max_idx - 1)) {
              retval += ", ";
          }
      }
      retval += "}";
  */

  return "";
}
#endif // __TILELINKLIB_HPP__
