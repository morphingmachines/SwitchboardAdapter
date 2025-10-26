// Copyright (c) 2024 Zero ASIC Corporation
// This code is licensed under Apache License 2.0 (see LICENSE for details)

#include "switchboard.hpp"
#include "tilelinklib.hpp"
#include <iostream>
#include <thread>
#define NBYTES 32

void send_thread(ClientTLAgent *client) {

  for (int j = 0; j < 32; j++) {
    int offset = j;
    TLMessageA tl_a;
    tl_a.opcode = PutFullData;
    tl_a.param = 0;
    tl_a.size = 2; // 32 bytes
    tl_a.corrupt = 0;
    tl_a.source = j;
    tl_a.address = offset * 4;
    tl_a.mask = 0xF;
    for (int i = 0; i < 4; i++) {
      tl_a.data[i] = i + offset * 4;
    }

    // send packet
    client->send_a(tl_a);
  }
}

void recv_thread(ClientTLAgent *client) {
  for (int j = 0; j < 32; j++) {

    TLMessageD tl_d;
    client->recv_d(tl_d);
    assert(tl_d.opcode == AccessAck);
    assert(tl_d.denied == 0);
  }
}

int main() {
  ClientTLAgent client("client_0");

  sb_packet txp;
  std::thread t(send_thread, &client);

  sb_packet rxp;
  std::thread r(recv_thread, &client);

  t.join();
  r.join();

  printf("PASS!\n");

  return 0;
}
