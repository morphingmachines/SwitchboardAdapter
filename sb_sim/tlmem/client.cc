// Copyright (c) 2024 Zero ASIC Corporation
// This code is licensed under Apache License 2.0 (see LICENSE for details)

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <iostream>
#include <thread>
#define NBYTES 32

void send_thread(SBTX *tx) {
  sb_packet txp;

  for (int j = 0; j < 32; j++) {
    int offset = j;
    TLMessageA *tl_a = (TLMessageA *)txp.data;
    tl_a->opcode = PutFullData;
    tl_a->params = 0;
    tl_a->size = 2; // 32 bytes
    tl_a->corrupt = 0;
    tl_a->source = j;
    tl_a->address = offset * 4;
    tl_a->mask = 0xF;
    for (int i = 0; i < 4; i++) {
      tl_a->data[i] = i + offset * 4;
    }

    txp.destination = 0xbeefcafe;
    txp.last = true;

    // send packet
    tx->send_blocking(txp);
    fprintf(stderr, "TX packet: %s\n", sb_packet_to_str(txp, NBYTES).c_str());
  }
}

void recv_thread(SBRX *rx) {
  sb_packet rxp;
  for (int j = 0; j < 32; j++) {
    while (rx->recv(rxp) == false)
      ;
    printf("RX packet: %s\n", sb_packet_to_str(rxp, NBYTES).c_str());
    TLMessageD *tl_d = (TLMessageD *)rxp.data;
    for (int i = 0; i < NBYTES; i++) {
      assert(tl_d->opcode == AccessAck);
      assert(tl_d->denied == 0);
    }
  }
}

int main() {
  SBTX tx;
  SBRX rx;

  // initialize connections
  fprintf(stderr, "client TX initializing\n");
  tx.init("client_0_a.q");
  fprintf(stderr, "client RX initializing\n");
  rx.init("client_0_d.q");

  // form packet

  sb_packet txp;
  std::thread t(send_thread, &tx);

  // receive packet

  sb_packet rxp;
  std::thread r(recv_thread, &rx);

  t.join();
  r.join();
  // send a packet that will end the test

  /*
  for (int i = 0; i < NBYTES; i++) {
    txp.data[i] = 0xff;
  }
  tx.send_blocking(txp);
  */

  // declare test as having passed for regression testing purposes

  printf("PASS!\n");

  return 0;
}
