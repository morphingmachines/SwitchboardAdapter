#ifndef __TL_MEMIFC_HPP__
#define __TL_MEMIFC_HPP__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include <fesvr/memif.h>

#include "tilelinklib.h"
#include "tilelinklib.hpp"

class ClientTLMemIfc : public chunked_memif_t {
public:
  ClientTLMemIfc(ClientTLAgent &agent) : tl_agent(agent) {
    tl_params = agent.get_TLBundleParams();
  }

  void read_chunk(addr_t taddr, size_t nbytes, void *dst) override {
    const size_t beat = chunk_align();
    assert((taddr & (beat - 1)) == 0 && "addr must be aligned to beat size");
    assert(nbytes >= beat && "Read size below minimum chunk size");
    assert(nbytes <= chunk_max_size() && "Read size exceeds maximum chunk size");
    assert(nbytes % beat == 0 && "Read size must be multiple of chunk_align()");

    uint8_t *dst_ptr = static_cast<uint8_t *>(dst);
    for_aligned_chunks(taddr, nbytes, [&](addr_t addr, size_t len) {
      read(addr, len, dst_ptr);
      dst_ptr += len;
    });
  }

  void write_chunk(addr_t taddr, size_t nbytes, const void *src) override {
    const size_t beat = chunk_align();
    assert((taddr & (beat - 1)) == 0 && "addr must be aligned to beat size");
    assert(nbytes >= beat && "Write size below minimum chunk size");
    assert(nbytes <= chunk_max_size() && "Write size exceeds maximum chunk size");
    assert(nbytes % beat == 0 && "Write size must be multiple of chunk_align()");

    const uint8_t *src_ptr = static_cast<const uint8_t *>(src);
    for_aligned_chunks(taddr, nbytes, [&](addr_t addr, size_t len) {
      write(addr, len, src_ptr);
      src_ptr += len;
    });
  }

  void clear_chunk(addr_t taddr, size_t nbytes) override {
    const size_t beat = chunk_align();
    const std::vector<uint8_t> zeros(beat, 0);
    for (size_t off = 0; off < nbytes; off += beat)
      write_chunk(taddr + off, beat, zeros.data());
  }

  size_t chunk_align() override { return (tl_params.data_bit_width / 8); }

  size_t chunk_max_size() override { return (tl_params.max_transfer_bytes); }

protected:
  ClientTLAgent &tl_agent;
  TLBundleParams tl_params;
  std::mutex lock_;

  // Largest power-of-2 <= n. Returns 0 for n == 0.
  static size_t floor_pow2(size_t n) {
    if (n == 0) return 0;
    return size_t(1) << (63 - __builtin_clzll(n));
  }

  // Walk [addr, addr+nbytes) as naturally-aligned power-of-2 sub-ranges.
  // Calls fn(sub_addr, sub_len) for each sub-range in order.
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

  // Validate that 'len' is power-of-two and 'addr' is aligned to 'len'.
  static void validate_len_addr(uint64_t addr, size_t len) {
    assert((len & (len - 1)) == 0 && "len must be power of 2");
    assert((addr & (len - 1)) == 0 && "addr must be aligned to len");
  }

  void read(uint64_t addr, size_t len, void *data) {
    std::lock_guard<std::mutex> guard(lock_);
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
      assert(tl_d.denied == 0 && "read denied");
      memcpy(dst + i * beat_bytes, tl_d.data, beat_bytes);
    }
  }

  void write(uint64_t addr, size_t len, const void *data) {
    std::lock_guard<std::mutex> guard(lock_);
    validate_len_addr(addr, len);
    const size_t beat_bytes = tl_params.data_bit_width / 8;
    assert(len >= beat_bytes && "len must be >= beat size");
    const size_t num_beats = len / beat_bytes;
    const uint8_t lg_size = static_cast<uint8_t>(__builtin_ctzll(len));

    const uint8_t *src = static_cast<const uint8_t *>(data);
    TLMessageA tl_a;
    for (size_t i = 0; i < num_beats; i++) {
      tl_agent.put(tl_a, 0, addr, lg_size, src + i * beat_bytes);
      tl_agent.send_a(tl_a);
    }

    TLMessageD tl_d;
    tl_agent.recv_d(tl_d);
    assert(tl_d.denied == 0 && "write denied");
  }

};
#endif // __TL_MEMIFC_HPP__
