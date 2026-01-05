#ifndef __TL_MEMIFC_HPP__
#define __TL_MEMIFC_HPP__

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <cstdint>
#include <iostream>
#include <string>

#include "tilelinklib.hpp"
#include <fesvr/memif.h>

class ClientTLMemIfc : public chunked_memif_t {
public:
  ClientTLMemIfc(ClientTLAgent &agent) : tl_agent(agent) {
    tl_params = agent.get_TLBundleParams();
  }

  void read_chunk(addr_t taddr, size_t nbytes, void *dst) override {
    size_t bytes_remaining = nbytes;
    uint8_t *dst_ptr = static_cast<uint8_t *>(dst);
    addr_t curr_addr = taddr;

    assert(nbytes >= chunk_align() && "Read size exceeds minimum chunk size");
    assert(nbytes <= chunk_max_size() &&
           "Read size exceeds maximum chunk size");

    while (bytes_remaining > 0) {
      size_t chunk_size = std::min(bytes_remaining, chunk_align());

      // Perform TileLink Get operation
      TLMessageA tlA;
      tlA.opcode = Get;
      tlA.param = 0;
      tlA.size = static_cast<uint8_t>(__builtin_ctz(chunk_size));
      tlA.address = curr_addr;
      tlA.source = 0; // Assuming single source
      tlA.mask = (1 << (chunk_size)) - 1;
      tlA.corrupt = 0;

      // tl_agent.print_a(tlA);
      tl_agent.send_a(tlA);

      TLMessageD tlD;
      tl_agent.recv_d(tlD);
      // tl_agent.print_d(tlD);

      // Copy data to destination
      size_t data_bytes = 1 << tlD.size;
      memcpy(dst_ptr, tlD.data, data_bytes);

      // Update pointers and counters
      dst_ptr += data_bytes;
      curr_addr += data_bytes;
      bytes_remaining -= data_bytes;
    }
  }

  void write_chunk(addr_t taddr, size_t nbytes, const void *src) override {
    size_t bytes_remaining = nbytes;
    const uint8_t *src_ptr = static_cast<const uint8_t *>(src);
    addr_t curr_addr = taddr;

    assert(nbytes >= chunk_align() && "Write size exceeds minimum chunk size");
    assert(nbytes <= chunk_max_size() &&
           "Write size exceeds maximum chunk size");

    while (bytes_remaining > 0) {
      size_t chunk_size = std::min(bytes_remaining, chunk_align());

      // Prepare TileLink Put operation
      TLMessageA tlA;
      tlA.opcode = PutFullData;
      tlA.param = 0;
      tlA.size = static_cast<uint8_t>(__builtin_ctz(chunk_size));
      tlA.address = curr_addr;
      tlA.source = 0; // Assuming single source
      tlA.mask = (1 << chunk_size) - 1;
      memcpy(tlA.data, src_ptr, chunk_size);
      tlA.corrupt = 0;

      // tl_agent.print_a(tlA);
      tl_agent.send_a(tlA);

      TLMessageD tlD;
      tl_agent.recv_d(tlD);
      // tl_agent.print_d(tlD);

      // Update pointers and counters
      src_ptr += chunk_size;
      curr_addr += chunk_size;
      bytes_remaining -= chunk_size;
    }
  }

  void clear_chunk(addr_t taddr, size_t nbytes) override {
    size_t chunk_size = chunk_align();
    std::vector<uint8_t> zero_data(chunk_size, 0);

    size_t bytes_remaining = nbytes;
    while (bytes_remaining > 0) {
      write_chunk(taddr, chunk_size, zero_data.data());
      taddr += chunk_size;
      bytes_remaining -= chunk_size;
    }
  }

  size_t chunk_align() override { return (tl_params.data_bit_width / 8); }

  size_t chunk_max_size() override { return (tl_params.data_bit_width / 8); }

protected:
  ClientTLAgent &tl_agent;
  TLBundleParams tl_params;
};
#endif // __TL_MEMIFC_HPP__
