/**
 * @file memifc.hpp
 * @brief TileLink memory interface adapter for FESVR's chunked_memif_t.
 *
 * Implements `chunked_memif_t` (from fesvr) on top of a `ClientTLAgent`,
 * translating bulk read/write requests into TileLink A/D channel transactions
 * over Switchboard queues.
 *
 * ## Usage
 * Construct with a configured `ClientTLAgent` (set_TLBundleParams already
 * called), then pass to any FESVR loader or memory-access driver that accepts
 * a `memif_t*`.
 *
 * ## Threading
 * `read_chunk`, `write_chunk`, and `clear_chunk` are mutex-protected and safe
 * to call concurrently. The private `read()` / `write()` helpers assume the
 * lock is already held by the caller.
 */

#ifndef __TL_MEMIFC_HPP__
#define __TL_MEMIFC_HPP__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include <fesvr/memif.h>

#include "tilelinklib.hpp"

/**
 * @brief chunked_memif_t implementation backed by a TileLink ClientTLAgent.
 *
 * Splits bulk accesses into naturally-aligned, power-of-2 sub-ranges
 * (each ≤ max_transfer_bytes) and issues one TileLink Get or PutFullData
 * burst per sub-range.
 */
class ClientTLMemIfc : public chunked_memif_t {
public:
  /**
   * @brief Construct and cache TL bundle parameters.
   * @param agent  Fully configured ClientTLAgent. Must outlive this object.
   *               set_TLBundleParams() must have been called on it already.
   */
  ClientTLMemIfc(ClientTLAgent &agent) : tl_agent(agent) {
    tl_params = agent.get_TLBundleParams();
  }

  /**
   * @brief Read @p nbytes from @p taddr into @p dst.
   *
   * Splits the range into aligned sub-ranges and issues one TL Get per
   * sub-range, collecting AccessAckData beats.
   *
   * @param taddr  Target address. Must be beat-aligned.
   * @param nbytes Byte count. Must be in [chunk_align(), chunk_max_size()]
   *               and a multiple of chunk_align().
   * @param dst    Destination buffer of at least @p nbytes bytes.
   */
  void read_chunk(addr_t taddr, size_t nbytes, void *dst) override {
    std::lock_guard<std::mutex> guard(lock_);
    const size_t beat = chunk_align();
    assert((taddr & (beat - 1)) == 0 && "addr must be aligned to beat size");
    assert(nbytes >= beat && "Read size below minimum chunk size");
    assert(nbytes <= chunk_max_size() &&
           "Read size exceeds maximum chunk size");
    assert(nbytes % beat == 0 && "Read size must be multiple of chunk_align()");

    uint8_t *dst_ptr = static_cast<uint8_t *>(dst);
    for_aligned_chunks(taddr, nbytes, [&](addr_t addr, size_t len) {
      read(addr, len, dst_ptr);
      dst_ptr += len;
    });
  }

  /**
   * @brief Write @p nbytes from @p src to @p taddr.
   *
   * Splits the range into aligned sub-ranges and issues one TL PutFullData
   * burst per sub-range, waiting for a single AccessAck per burst.
   *
   * @param taddr  Target address. Must be beat-aligned.
   * @param nbytes Byte count. Must be in [chunk_align(), chunk_max_size()]
   *               and a multiple of chunk_align().
   * @param src    Source buffer of at least @p nbytes bytes.
   */
  void write_chunk(addr_t taddr, size_t nbytes, const void *src) override {
    std::lock_guard<std::mutex> guard(lock_);
    const size_t beat = chunk_align();
    assert((taddr & (beat - 1)) == 0 && "addr must be aligned to beat size");
    assert(nbytes >= beat && "Write size below minimum chunk size");
    assert(nbytes <= chunk_max_size() &&
           "Write size exceeds maximum chunk size");
    assert(nbytes % beat == 0 &&
           "Write size must be multiple of chunk_align()");

    const uint8_t *src_ptr = static_cast<const uint8_t *>(src);
    for_aligned_chunks(taddr, nbytes, [&](addr_t addr, size_t len) {
      write(addr, len, src_ptr);
      src_ptr += len;
    });
  }

  /**
   * @brief Zero-fill [@p taddr, @p taddr + @p nbytes).
   *
   * Writes one beat-sized zero block at a time via write_chunk().
   *
   * @param taddr  Target address. Must be beat-aligned.
   * @param nbytes Byte count. Must be a multiple of chunk_align().
   */
  void clear_chunk(addr_t taddr, size_t nbytes) override {
    const size_t max_chunk = chunk_max_size();
    std::vector<uint8_t> zeros_(max_chunk, 0);
    addr_t curr = taddr;
    size_t remaining = nbytes;
    while (remaining > 0) {
      size_t len = std::min(remaining, max_chunk);
      write_chunk(curr, len, zeros_.data());
      curr += len;
      remaining -= len;
    }
  }

  /// @brief Returns the minimum transfer granularity in bytes (one TL beat).
  size_t chunk_align() override { return (tl_params.data_bit_width / 8); }

  /// @brief Returns the maximum transfer size in bytes (TL max burst).
  size_t chunk_max_size() override { return (tl_params.max_transfer_bytes); }

protected:
  ClientTLAgent &tl_agent;  ///< Underlying TL agent (not owned).
  TLBundleParams tl_params; ///< Cached TL bundle parameters.

private:
  std::mutex lock_;

  /**
   * @brief Returns the largest power-of-2 ≤ @p n.
   * @param n Must be > 0.
   */
  static size_t floor_pow2(size_t n) {
    assert(n > 0 && "floor_pow2 called with n == 0");
    return size_t(1) << (63 - __builtin_clzll(n));
  }

  /**
   * @brief Walk [@p addr, @p addr + @p nbytes) as naturally-aligned,
   *        power-of-2 sub-ranges, invoking @p fn for each.
   *
   * Each sub-range satisfies: base is aligned to its own size, size is a
   * power-of-2. Sub-ranges are emitted in address order.
   *
   * @param addr   Start address.
   * @param nbytes Total byte count (> 0).
   * @param fn     Callable with signature `void(addr_t sub_addr, size_t
   * sub_len)`.
   */
  template <typename Fn>
  static void for_aligned_chunks(addr_t addr, size_t nbytes, Fn fn) {
    addr_t curr = addr;
    size_t remaining = nbytes;
    while (remaining > 0) {
      size_t chunk = floor_pow2(remaining);
      while (curr & (chunk - 1))
        chunk >>= 1;
      fn(curr, chunk);
      curr += chunk;
      remaining -= chunk;
    }
  }

  /**
   * @brief Assert @p len is a power-of-2 and @p addr is aligned to @p len.
   */
  static void validate_len_addr(uint64_t addr, size_t len) {
    assert((len & (len - 1)) == 0 && "len must be power of 2");
    assert((addr & (len - 1)) == 0 && "addr must be aligned to len");
  }

  /**
   * @brief Issue a TL Get for [@p addr, @p addr + @p len) and collect response
   * beats.
   *
   * Sends one A-channel Get, then receives @p len / beat_bytes AccessAckData
   * beats and copies data into @p data.
   *
   * @param addr  Base address. Must be power-of-2-aligned to @p len.
   * @param len   Transfer size in bytes. Must be a power-of-2 ≥ beat size.
   * @param data  Output buffer of at least @p len bytes.
   */
  void read(uint64_t addr, size_t len, void *data) {
    validate_len_addr(addr, len);
    const size_t beat_bytes = tl_params.data_bit_width / 8;
    assert(len >= beat_bytes && "len must be >= beat size");
    const size_t num_beats = len / beat_bytes;
    const uint8_t lg_size = static_cast<uint8_t>(__builtin_ctzll(len));

    TLMessageA tl_a;
    tl_agent.get(tl_a, 0, addr, lg_size);
    tl_agent.send_a(tl_a);

    uint8_t *dst = static_cast<uint8_t *>(data);
    TLMessageD tl_d;
    for (size_t i = 0; i < num_beats; i++) {
      tl_agent.recv_d(tl_d);
      assert(tl_d.opcode == AccessAckData &&
             "read: unexpected D-channel opcode");
      assert(tl_d.denied == 0 && "read denied");
      memcpy(dst + i * beat_bytes, tl_d.data, beat_bytes);
    }
  }

  /**
   * @brief Issue a TL PutFullData burst for [@p addr, @p addr + @p len) and
   *        wait for the AccessAck.
   *
   * Sends @p len / beat_bytes A-channel beats (all with the same base address
   * and lg_size, per TL burst semantics), then receives one AccessAck.
   *
   * @param addr  Base address. Must be power-of-2-aligned to @p len.
   * @param len   Transfer size in bytes. Must be a power-of-2 ≥ beat size.
   * @param data  Source buffer of at least @p len bytes.
   */
  void write(uint64_t addr, size_t len, const void *data) {
    validate_len_addr(addr, len);
    const size_t beat_bytes = tl_params.data_bit_width / 8;
    assert(len >= beat_bytes && "len must be >= beat size");
    const size_t num_beats = len / beat_bytes;
    const uint8_t lg_size = static_cast<uint8_t>(__builtin_ctzll(len));

    const uint8_t *src = static_cast<const uint8_t *>(data);
    TLMessageA tl_a;
    for (size_t i = 0; i < num_beats; i++) {
      tl_agent.put(tl_a, 0, addr, lg_size, src + i * beat_bytes);
      // tl_agent.print_a(tl_a); // Debug print
      tl_agent.send_a(tl_a);
    }

    TLMessageD tl_d;
    tl_agent.recv_d(tl_d);
    assert(tl_d.opcode == AccessAck && "write: unexpected D-channel opcode");
    assert(tl_d.denied == 0 && "write denied");
    // tl_agent.print_d(tl_d); // Debug print
  }
};
#endif // __TL_MEMIFC_HPP__
