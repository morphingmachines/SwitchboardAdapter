#ifndef __TILELINKLIB_HPP__
#define __TILELINKLIB_HPP__

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <cstdint>
#include <iostream>
#include <string>

static inline int first_bit_set_u32(uint32_t v) {
  if (v == 0)
    return -1;
  return __builtin_ctz(v);
}

static inline std::string data_to_bytes(const uint8_t *data, int data_width) {
  std::string result;
  int num_bytes = data_width / 8;
  assert((data_width % 8) == 0);
  assert(num_bytes != 0 &&
         (num_bytes & (num_bytes - 1)) == 0); // Check for Power_of_two
  char byte_str[4];

  result = "[";
  for (int i = 0; i < num_bytes; i++) {
    snprintf(byte_str, sizeof(byte_str), "%02x", data[i]);
    result += byte_str;
    if (i < num_bytes - 1)
      result += " ";
  }
  result += "]";
  return result;
}

static inline std::string tlA_to_str(const TLMessageA &msg,
                                     const TLBundleParams &p) {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "TL-A[op=%s, param=%d, size=%d, source=%d, corrupt=%d, addr=0x%lx, "
           "mask=0x%x, data=%s]",
           get_opcodeA_str(msg.opcode).c_str(), msg.param, msg.size, msg.source,
           msg.corrupt, msg.address, msg.mask,
           data_to_bytes(msg.data, p.data_bit_width).c_str());
  return std::string(buf);
}

static inline std::string tlD_to_str(const TLMessageD &msg,
                                     const TLBundleParams &p) {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "TL-D[op=%s, param=%d, size=%d, source=%d, sink=%d, corrupt=%d, "
           "denied=%d, data=%s]",
           get_opcodeD_str(msg.opcode).c_str(), msg.param, msg.size, msg.source,
           msg.sink, msg.corrupt, msg.denied,
           data_to_bytes(msg.data, p.data_bit_width).c_str());
  return std::string(buf);
}

class TLAgent {
public:
  TLAgent() : p_set(false), info("") {}

  void set_TLBundleParams(const std::string &name, uint32_t dataWidth,
                          uint8_t addrWidth, uint8_t srcWidth,
                          uint8_t lgSizeWidth) {
    assert(!p_set && "TLBundleParams already set!");
    info = name;
    p = {.address_bit_width = addrWidth,
         .source_bit_width = srcWidth,
         .sink_bit_width = 1,
         .size_bit_width = lgSizeWidth,
         .data_bit_width = dataWidth};
    p_set = true;
  }

  TLBundleParams get_TLBundleParams() const {
    assert(p_set && "TLBundleParams not set!");
    return p;
  }

  void putPartial(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
                  uint8_t lgSize, uint32_t mask, const uint8_t *data) {
    assert(p_set && "TLBundleParams not set!");
    int size = 1 << lgSize;
    msg.opcode = PutPartialData;
    msg.param = 0;
    msg.size = lgSize;
    msg.address = toAddress;
    msg.mask = alignMask(mask, lgSize, toAddress);
    int i = first_bit_set_u32(msg.mask);
    memcpy(&msg.data[i], data, size);
    msg.corrupt = 0;
  }

  void put(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
           uint8_t lgSize, const uint8_t *data) {
    assert(p_set && "TLBundleParams not set!");
    int size = 1 << lgSize;
    if (size == p.data_bit_width / 8) {
      msg.opcode = PutFullData;
    } else {
      msg.opcode = PutPartialData;
    }
    msg.param = 0;
    msg.size = lgSize;
    msg.address = toAddress;
    msg.mask = alignMask(lgSize, toAddress);
    int i = first_bit_set_u32(msg.mask);
    memcpy(&msg.data[i], data, size);
    msg.corrupt = 0;
  }

  void get(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
           uint8_t lgSize) {
    msg.opcode = Get;
    msg.param = 0;
    msg.size = lgSize;
    msg.address = toAddress;
    msg.mask = alignMask(lgSize, toAddress);
    msg.corrupt = 0;
  }

  void accessAckData(TLMessageD &msg, uint32_t toSource, uint8_t lgSize,
                     const uint8_t *data, uint32_t denied,
                     uint32_t alignedMask) {
    msg.opcode = AccessAckData;
    msg.param = 0;
    msg.size = lgSize;
    msg.source = toSource;
    msg.sink = 0;
    int i = first_bit_set_u32(alignedMask);
    int size = 1 << lgSize;
    memcpy(msg.data, data, size);
    msg.corrupt = 0;
    msg.denied = denied;
  }

  void accessAck(TLMessageD &msg, uint32_t toSource, uint8_t lgSize,
                 uint32_t denied) {
    msg.opcode = AccessAck;
    msg.param = 0;
    msg.size = lgSize;
    msg.source = toSource;
    msg.sink = 0;
    msg.corrupt = 0;
    msg.denied = denied;
  }

protected:
  std::string info;
  TLBundleParams p;
  bool p_set;

  uint32_t alignMask(uint32_t lgSize, uint64_t byteAddress) {
    assert(p_set && "TLBundleParams not set!");
    uint32_t size = 1 << lgSize;
    uint32_t unalignedMask = (1 << size) - 1;
    return alignMask(unalignedMask, lgSize, byteAddress);
  }

  uint32_t alignMask(uint32_t unalignedMask, uint32_t lgSize,
                     uint64_t byteAddress) {
    uint32_t offset = byteAddress & ((p.data_bit_width / 8) - 1);
    uint32_t size = 1 << lgSize;
    assert(size <= (p.data_bit_width / 8) && "Size exceeds data width!");
    assert((offset & (size - 1)) == 0 && "Address not aligned to size!");
    uint32_t slotId = offset >> lgSize;
    uint32_t alignedMask = unalignedMask << (slotId * size);
    return alignedMask;
  }
};

// Client Agent class (Master/Source)
class ClientTLAgent : public TLAgent {
public:
  ClientTLAgent(const std::string &uri, size_t capacity = 0, bool fresh = false,
                double max_rate = -1) {

    a_tx.init(uri + "_a.q", capacity, fresh, max_rate);
    d_rx.init(uri + "_d.q", capacity, fresh, max_rate);
    info = uri;
    p = {
        .address_bit_width = 64,
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

  void print_a(const TLMessageA &msg) const {
    std::cout << info << ":" << tlA_to_str(msg, p) << std::endl;
  }

  void print_d(const TLMessageD &msg) const {
    std::cout << info << ":" << tlD_to_str(msg, p) << std::endl;
  }

private:
  SBTX a_tx; // A channel
  SBRX d_rx; // D channel
};

// Manager Agent class (Slave/Sink)
class ManagerTLAgent : public TLAgent {
public:
  ManagerTLAgent(const std::string &uri, size_t capacity = 0,
                 bool fresh = false, double max_rate = -1) {
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

  void print_a(const TLMessageA &msg) const {
    std::cout << info << ":" << tlA_to_str(msg, p) << std::endl;
  }

  void print_d(const TLMessageD &msg) const {
    std::cout << info << ":" << tlD_to_str(msg, p) << std::endl;
  }

private:
  SBRX a_rx; // A channel
  SBTX d_tx; // D channel
};

#endif // __TILELINKLIB_HPP__
