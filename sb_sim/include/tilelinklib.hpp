#ifndef __TILELINKLIB_HPP__
#define __TILELINKLIB_HPP__

/**
 * @file tilelinklib.hpp
 * @brief TileLink agent helpers for Switchboard-based C++ testbenches.
 *
 * Provides TLAgent (base), ClientTLAgent (master), and ManagerTLAgent (target)
 * for driving TileLink A/D channels over Switchboard queues.
 *
 * ## Supported TileLink variants
 * - TL-UL and TL-UH (A and D channels only).
 * - TL-C (channels B, C, E) is not yet supported.
 *
 * ## Implementation constraints
 * - `data_bit_width` is capped at 256 bits: TLMessageA/D carry fixed 32-byte
 *   payload arrays, so a 256-bit bus exactly fills them.
 * - `lgSize` is capped at 12 (4096 bytes): asserts in put/get/putPartial guard
 *   against shifts that would overflow a 32-bit int.
 * - send_a / recv_d / recv_a / send_d are blocking (spin until queue ready).
 * - Each agent instance must be accessed from a single thread.
 * - set_TLBundleParams() must be called exactly once before any transaction.
 */

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

/**
 * @brief Returns the A-channel opcode as a human-readable string.
 * @param opcode TLAOpcode value.
 */
inline std::string get_opcodeA_str(TLAOpcode opcode) {
  switch (opcode) {
  case PutFullData:    return "PutFullData";
  case PutPartialData: return "PutPartialData";
  case ArithmeticData: return "ArithmeticData";
  case LogicalData:    return "LogicalData";
  case Get:            return "Get";
  case Hint:           return "Hint";
  default:             return "Unknown";
  }
}

/**
 * @brief Returns the D-channel opcode as a human-readable string.
 * @param opcode TLDOpcode value.
 */
inline std::string get_opcodeD_str(TLDOpcode opcode) {
  switch (opcode) {
  case AccessAck:     return "AccessAck";
  case AccessAckData: return "AccessAckData";
  case HintAck:       return "HintAck";
  default:            return "Unknown";
  }
}

/**
 * @brief Returns the index of the lowest set bit in @p v.
 * @return Bit index [0..31], or -1 if @p v is zero.
 */
static inline int first_bit_set_u32(uint32_t v) {
  if (v == 0)
    return -1;
  return __builtin_ctz(v);
}

/**
 * @brief Formats a byte array as a hex string "[xx xx ...]".
 * @param data       Pointer to byte array.
 * @param data_width Width in bits; must be a non-zero power-of-2 multiple of 8.
 */
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

/**
 * @brief Formats a TL-A message as a human-readable string.
 * @param msg TL-A message to format.
 * @param p   Bundle parameters (used for data_bit_width).
 */
static inline std::string tlA_to_str(const TLMessageA &msg,
                                     const TLBundleParams &p) {
  std::string result = "TL-A[op=" + get_opcodeA_str(static_cast<TLAOpcode>(msg.opcode));
  result += ", param=" + std::to_string(msg.param);
  result += ", size=" + std::to_string(msg.size);
  result += ", source=" + std::to_string(msg.source);
  result += ", corrupt=" + std::to_string(msg.corrupt);
  char addr_buf[32];
  snprintf(addr_buf, sizeof(addr_buf), ", addr=0x%lx", msg.address);
  result += addr_buf;
  char mask_buf[16];
  snprintf(mask_buf, sizeof(mask_buf), ", mask=0x%x", msg.mask);
  result += mask_buf;
  result += ", data=" + data_to_bytes(msg.data, p.data_bit_width) + "]";
  return result;
}

/**
 * @brief Formats a TL-D message as a human-readable string.
 * @param msg TL-D message to format.
 * @param p   Bundle parameters (used for data_bit_width).
 */
static inline std::string tlD_to_str(const TLMessageD &msg,
                                     const TLBundleParams &p) {
  std::string result = "TL-D[op=" + get_opcodeD_str(static_cast<TLDOpcode>(msg.opcode));
  result += ", param=" + std::to_string(msg.param);
  result += ", size=" + std::to_string(msg.size);
  result += ", source=" + std::to_string(msg.source);
  result += ", sink=" + std::to_string(msg.sink);
  result += ", corrupt=" + std::to_string(msg.corrupt);
  result += ", denied=" + std::to_string(msg.denied);
  result += ", data=" + data_to_bytes(msg.data, p.data_bit_width) + "]";
  return result;
}

/**
 * @brief Base class for TileLink agents.
 *
 * Holds TLBundleParams and provides helpers to construct TL-A and TL-D
 * messages.  Does not own Switchboard queues; subclasses add transport.
 */
class TLAgent {
public:
  TLAgent() : info(""), p_set(false) {}

  /**
   * @brief Configure bundle parameters for a TL-UL (single-beat) link.
   *
   * `size_bit_width` and `max_transfer_bytes` are derived from @p dataWidth
   * (both equal one beat).  Use the TLBundleParams overload for multi-beat links.
   *
   * @param name      Label used in log output.
   * @param dataWidth Bus width in bits.  Must be a power-of-2 multiple of 8, <= 256.
   * @param addrWidth Address field width in bits.
   * @param srcWidth  Source ID field width in bits.
   */
  void set_TLBundleParams(const std::string &name, uint32_t dataWidth,
                          uint8_t addrWidth, uint8_t srcWidth) {
    // TL-UL: single-beat only; lgSizeWidth derived from dataWidth.
    assert(!p_set && "TLBundleParams already set!");
    assert(dataWidth <= 256 && "Data width exceeds 256 bits!");
    assert((dataWidth % 8) == 0 && "Data width must be byte-aligned!");
    info = name;
    p = {.address_bit_width = addrWidth,
         .source_bit_width = srcWidth,
         .sink_bit_width = 1,
         .size_bit_width = static_cast<uint8_t>(__builtin_ctz(dataWidth / 8)),
         .data_bit_width = dataWidth,
         .max_transfer_bytes = dataWidth / 8};
    p_set = true;
  }

  /**
   * @brief Configure bundle parameters with explicit control.
   *
   * Use when the convenience TL-UL overload does not fit — e.g. TL-UH with
   * non-default max_transfer_bytes, or TL-UL with specific sink/size widths.
   *
   * @param name   Label used in log output.
   * @param params Fully specified bundle parameters.
   *               `data_bit_width` <= 256, `max_transfer_bytes` <= 4096.
   */
  void set_TLBundleParams(const std::string &name,
                          const TLBundleParams &params) {
    assert(!p_set && "TLBundleParams already set!");
    assert(params.data_bit_width <= 256 && "Data width exceeds 256 bits!");
    assert(params.max_transfer_bytes <= 4096 &&
           "Max transfer bytes exceeds 4096!");
    info = name;
    p = params;
    p_set = true;
  }

  /**
   * @brief Returns the configured bundle parameters.
   */
  TLBundleParams get_TLBundleParams() const {
    assert(p_set && "TLBundleParams not set!");
    return p;
  }

  /**
   * @brief Fill a TL-A PutPartialData message.
   *
   * @param msg        Output message.
   * @param fromSource Source ID.
   * @param toAddress  Target byte address.
   * @param lgSize     Log2 of transfer size in bytes (<= 12).
   * @param mask       Byte-enable mask relative to transfer base.
   * @param data       Payload; caller provides (1<<lgSize) bytes starting at data[0].
   *                   This method shifts the bytes to the correct byte-lane in msg.data.
   *
   * @note Full-beat partial (`(1<<lgSize) >= beat_bytes`): @p mask passed
   *       through unchanged (no lane shift needed).
   *       Sub-beat: @p mask is shifted to the correct lane within the beat.
   */
  void putPartial(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
                  uint8_t lgSize, uint32_t mask, const uint8_t *data) {
    assert(p_set && "TLBundleParams not set!");
    assert(lgSize <= 12 && "lgSize exceeds 4096 bytes!");
    assert((1u << lgSize) <= p.max_transfer_bytes &&
           "lgSize exceeds max transfer bytes!");
    int size = 1 << lgSize;
    msg.opcode = PutPartialData;
    msg.param = 0;
    msg.size = lgSize;
    msg.address = toAddress;
    msg.source = fromSource;
    if (mask == 0) {
      msg.mask = 0;
    } else if ((1u << lgSize) >= (p.data_bit_width / 8)) {
      msg.mask = mask;
      memcpy(&msg.data[0], data, p.data_bit_width / 8);
    } else {
      msg.mask = alignMask(mask, lgSize, toAddress);
      int i = first_bit_set_u32(msg.mask);
      memcpy(&msg.data[i], data, size);
    }
    msg.corrupt = 0;
  }

  /**
   * @brief Fill a TL-A PutFullData message.
   *
   * @param msg        Output message.
   * @param fromSource Source ID.
   * @param toAddress  Target byte address.
   * @param lgSize     Log2 of transfer size in bytes (<= 12).
   * @param data       Payload pointer.
   *
   * @note Caller always provides data starting at data[0].  This method
   *       internally shifts the bytes to the correct byte-lane within the beat
   *       based on @p lgSize and @p toAddress.
   */
  void put(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
           uint8_t lgSize, const uint8_t *data) {
    assert(p_set && "TLBundleParams not set!");
    assert(lgSize <= 12 && "lgSize exceeds 4096 bytes!");
    assert((1u << lgSize) <= p.max_transfer_bytes &&
           "lgSize exceeds max transfer bytes!");
    int beat_bytes = p.data_bit_width / 8;
    msg.opcode = PutFullData;
    msg.param = 0;
    msg.size = lgSize;
    msg.mask = alignMask(lgSize, toAddress);
    if((1 << lgSize) < beat_bytes) {
      int i = first_bit_set_u32(msg.mask);
      memcpy(&msg.data[i], data, 1 << lgSize);
    } else {
      memcpy(&msg.data[0], data, beat_bytes);
    }
    msg.address = toAddress;
    msg.source = fromSource;
    msg.corrupt = 0;
  }

  /**
   * @brief Fill a TL-A Get message.
   *
   * @param msg        Output message.
   * @param fromSource Source ID.
   * @param toAddress  Target byte address.
   * @param lgSize     Log2 of transfer size in bytes (<= 12).
   */
  void get(TLMessageA &msg, uint32_t fromSource, uint64_t toAddress,
           uint8_t lgSize) {
    assert(p_set && "TLBundleParams not set!");
    assert(lgSize <= 12 && "lgSize exceeds 4096 bytes!");
    assert((1u << lgSize) <= p.max_transfer_bytes &&
           "lgSize exceeds max transfer bytes!");
    msg.opcode = Get;
    msg.param = 0;
    msg.size = lgSize;
    msg.address = toAddress;
    msg.source = fromSource;
    msg.mask = alignMask(lgSize, toAddress);
    msg.corrupt = 0;
  }

  /**
   * @brief Fill a TL-D AccessAckData response.
   *
   * @param msg      Output message.
   * @param toSource Source ID echoed from the corresponding Get request.
   * @param lgSize   Log2 of transfer size in bytes (echo from the Get).
   * @param data     Response payload; caller provides at least beat_bytes bytes.
   * @param denied   1 if request denied, else 0.
   *
   * @note Always copies beat_bytes from data[0] into msg.data[0].
   *       The receiver uses the mask from the original Get to identify valid lanes.
   */
  void accessAckData(TLMessageD &msg, uint32_t toSource, uint8_t lgSize,
                     const uint8_t *data, uint32_t denied) {
    assert(p_set && "TLBundleParams not set!");
    assert(lgSize <= 12 && "lgSize exceeds 4096 bytes!");
    assert((1u << lgSize) <= p.max_transfer_bytes &&
           "lgSize exceeds max transfer bytes!");
    msg.opcode = AccessAckData;
    msg.param = 0;
    msg.size = lgSize;
    msg.source = toSource;
    msg.sink = 0;
    int beat_bytes = p.data_bit_width / 8;
    memcpy(&msg.data[0], data, beat_bytes);
    msg.corrupt = 0;
    msg.denied = denied;
  }

  /**
   * @brief Fill a TL-D AccessAck response (write acknowledgement, no data).
   *
   * @param msg      Output message.
   * @param toSource Source ID echoed from the corresponding Put request.
   * @param lgSize   Log2 of transfer size in bytes (echo from the Put).
   * @param denied   1 if request denied, else 0.
   */
  void accessAck(TLMessageD &msg, uint32_t toSource, uint8_t lgSize,
                 uint32_t denied) {
    assert(p_set && "TLBundleParams not set!");
    msg.opcode = AccessAck;
    msg.param = 0;
    msg.size = lgSize;
    msg.source = toSource;
    msg.sink = 0;
    msg.corrupt = 0;
    msg.denied = denied;
  }

protected:
  std::string info;  ///< Label for log output (set to URI at construction).
  TLBundleParams p;  ///< Configured bundle parameters.
  bool p_set;        ///< True once set_TLBundleParams() has been called.

  /**
   * @brief Compute a naturally-aligned byte-enable mask for @p lgSize at @p byteAddress.
   *
   * Returns all-ones when transfer size >= beat; otherwise delegates to the
   * two-argument overload to shift the mask to the correct slot.
   */
  uint32_t alignMask(uint32_t lgSize, uint64_t byteAddress) {
    assert(p_set && "TLBundleParams not set!");
    uint32_t size = 1 << lgSize;
    uint8_t beatBytes = p.data_bit_width / 8;
    if (size >= beatBytes) {
      return (((uint64_t)1 << beatBytes) - 1);
    }
    uint32_t unalignedMask = (((uint64_t)1 << size) - 1);
    return alignMask(unalignedMask, lgSize, byteAddress);
  }

  /**
   * @brief Shift @p unalignedMask to the correct byte-lane slot within a beat.
   *
   * Computes `slotId = (byteAddress % beat_bytes) >> lgSize` and returns
   * `unalignedMask << (slotId * (1<<lgSize))`.
   *
   * @param unalignedMask Mask relative to transfer base (bit 0 = first byte of transfer).
   * @param lgSize        Log2 of transfer size; must satisfy (1<<lgSize) <= beat_bytes.
   * @param byteAddress   Must be aligned to (1<<lgSize).
   */
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

/**
 * @brief TileLink client (master/initiator) agent.
 *
 * Owns an A-channel TX queue and a D-channel RX queue backed by Switchboard.
 *
 * @note Queue files opened at construction: `{uri}_a.q` (TX) and `{uri}_d.q` (RX).
 */
class ClientTLAgent : public TLAgent {
public:
  /**
   * @brief Construct and open Switchboard queues.
   *
   * @param uri       Base name for queue files.
   * @param capacity  Queue capacity (0 = Switchboard default).
   * @param fresh     If true, recreate queues from scratch.
   * @param max_rate  Max packet rate in packets/s (-1 = unlimited).
   *
   * @note Default bundle params assume a 256-bit bus.
   *       Call set_TLBundleParams() before issuing transactions.
   */
  ClientTLAgent(const std::string &uri, size_t capacity = 0, bool fresh = false,
                double max_rate = -1) {
    a_tx.init(uri + "_a.q", capacity, fresh, max_rate);
    d_rx.init(uri + "_d.q", capacity, fresh, max_rate);
    info = uri;
    // Default params — caller must invoke set_TLBundleParams() before use.
    p = {
        .address_bit_width = 64,
        .source_bit_width = 32,
        .sink_bit_width = 32,
        .size_bit_width = 4,
        .data_bit_width = 256,
        .max_transfer_bytes = 32,
    };
  }

  /**
   * @brief Send a TL-A message on the A channel (blocking).
   * @param tlA Message to send; fill using put(), putPartial(), or get().
   */
  void send_a(const TLMessageA &tlA) {
    sb_packet packet;
    memcpy(&packet.data, &tlA, sizeof(tlA));
    a_tx.send_blocking(packet);
  }

  /**
   * @brief Receive a TL-D response on the D channel (blocking).
   * @param tlD Output: filled with the received TL-D message.
   */
  void recv_d(TLMessageD &tlD) {
    sb_packet packet;
    d_rx.recv_blocking(packet);
    memcpy(&tlD, &packet.data, sizeof(tlD));
  }

  /** @brief Print a TL-A message to stdout prefixed with the agent label. */
  void print_a(const TLMessageA &msg) const {
    std::cout << info << ":" << tlA_to_str(msg, p) << std::endl;
  }

  /** @brief Print a TL-D message to stdout prefixed with the agent label. */
  void print_d(const TLMessageD &msg) const {
    std::cout << info << ":" << tlD_to_str(msg, p) << std::endl;
  }

private:
  SBTX a_tx; ///< A channel transmit queue.
  SBRX d_rx; ///< D channel receive queue.
};

/**
 * @brief TileLink manager (slave/target) agent.
 *
 * Owns an A-channel RX queue and a D-channel TX queue backed by Switchboard.
 *
 * @note Queue files opened at construction: `{uri}_a.q` (RX) and `{uri}_d.q` (TX).
 */
class ManagerTLAgent : public TLAgent {
public:
  /**
   * @brief Construct and open Switchboard queues.
   *
   * @param uri       Base name for queue files.
   * @param capacity  Queue capacity (0 = Switchboard default).
   * @param fresh     If true, recreate queues from scratch.
   * @param max_rate  Max packet rate in packets/s (-1 = unlimited).
   *
   * @note Default bundle params assume a 256-bit bus.
   *       Call set_TLBundleParams() before issuing transactions.
   */
  ManagerTLAgent(const std::string &uri, size_t capacity = 0,
                 bool fresh = false, double max_rate = -1) {
    a_rx.init(uri + "_a.q", capacity, fresh, max_rate);
    d_tx.init(uri + "_d.q", capacity, fresh, max_rate);
    info = uri;
    // Default params — caller must invoke set_TLBundleParams() before use.
    p = {.address_bit_width = 64,
         .source_bit_width = 32,
         .sink_bit_width = 32,
         .size_bit_width = 4,
         .data_bit_width = 256,
         .max_transfer_bytes = 32};
  }

  /**
   * @brief Receive a TL-A request on the A channel (blocking).
   * @param tlA Output: filled with the received TL-A message.
   */
  void recv_a(TLMessageA &tlA) {
    sb_packet packet;
    a_rx.recv_blocking(packet);
    memcpy(&tlA, &packet.data, sizeof(tlA));
  }

  /**
   * @brief Send a TL-D response on the D channel (blocking).
   * @param tlD Message to send; fill using accessAck() or accessAckData().
   */
  void send_d(const TLMessageD &tlD) {
    sb_packet packet;
    memcpy(&packet.data, &tlD, sizeof(tlD));
    d_tx.send_blocking(packet);
  }

  /** @brief Print a TL-A message to stdout prefixed with the agent label. */
  void print_a(const TLMessageA &msg) const {
    std::cout << info << ":" << tlA_to_str(msg, p) << std::endl;
  }

  /** @brief Print a TL-D message to stdout prefixed with the agent label. */
  void print_d(const TLMessageD &msg) const {
    std::cout << info << ":" << tlD_to_str(msg, p) << std::endl;
  }

private:
  SBRX a_rx; ///< A channel receive queue.
  SBTX d_tx; ///< D channel transmit queue.
};

#endif // __TILELINKLIB_HPP__