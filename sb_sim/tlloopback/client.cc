// Copyright (c) 2024 Zero ASIC Corporation
// This code is licensed under Apache License 2.0 (see LICENSE for details)

#include "switchboard.hpp"
#include "tilelinklib.h"
#include <iostream>
#include <thread>
#define NBYTES 32

void client_thread(SBTX *sb_tlA, SBRX *sb_tlD) {
  sb_packet packetA;

  TLMessageA *tl_a = (TLMessageA *)packetA.data;
  tl_a->opcode = PutFullData;
  tl_a->param = 0;
  tl_a->size = 4; // 8 bytes
  tl_a->corrupt = 0;
  tl_a->source = 0;
  tl_a->address = 0xDEAD0;
  tl_a->mask = 0xFF;
  for (int i = 0; i < 8; i++) {
    tl_a->data[i] = i;
  }

  packetA.destination = 0xbeefcafe;
  packetA.last = true;

  // send packet
  sb_tlA->send_blocking(packetA);
  fprintf(stderr, "client TX A packet: %s\n", sb_packet_to_str(packetA, NBYTES).c_str());

  //TLMessageA *tl_a = (TLMessageA *)packetA.data;
  tl_a->opcode = PutFullData;
  tl_a->param = 0;
  tl_a->size = 4; // 8 bytes
  tl_a->corrupt = 0;
  tl_a->source = 0;
  tl_a->address = 0xDEAD0;
  tl_a->mask = 0xFF;
  for (int i = 0; i < 8; i++) {
    tl_a->data[i] = i;
  }

  packetA.destination = 0xbeefcafe;
  packetA.last = true;

  // send packet
  sb_tlA->send_blocking(packetA);
  fprintf(stderr, "client TX A packet: %s\n", sb_packet_to_str(packetA, NBYTES).c_str());

  sb_packet packetD;
  while (sb_tlD->recv(packetD) == false)
    ;
  fprintf(stderr, "Client RX D packet: %s\n", sb_packet_to_str(packetD, NBYTES).c_str());
  TLMessageD *tl_d = (TLMessageD *)packetD.data;
  assert(tl_d->opcode == AccessAck);
  assert(tl_d->denied == 0);
}

void manager_thread(SBRX *sb_tlA, SBTX *sb_tlD) {
  sb_packet packetA;
  while (sb_tlA->recv(packetA) == false)
    ;
  printf("Manager RX A packet: %s\n", sb_packet_to_str(packetA, NBYTES).c_str());
  TLMessageA *tl_a = (TLMessageA *)packetA.data;
  assert(tl_a->opcode == PutFullData);
  assert(tl_a->corrupt == 0);

  while (sb_tlA->recv(packetA) == false)
    ;
  printf("Manager RX A packet: %s\n", sb_packet_to_str(packetA, NBYTES).c_str());
  //TLMessageA *tl_a = (TLMessageA *)packetA.data;
  assert(tl_a->opcode == PutFullData);
  assert(tl_a->corrupt == 0);

  sb_packet packetD;

  TLMessageD *tl_d = (TLMessageD *)packetD.data;
  tl_d->opcode = AccessAck;
  tl_d->param = 0;
  tl_d->size = 4; // 8 bytes
  tl_d->corrupt = 0;
  tl_d->denied = 0;
  tl_d->source = tl_a->source;
  tl_d->sink = 0;

  packetA.destination = 0xbeefcafe;
  packetA.last = true;

  // send packet
  sb_tlD->send_blocking(packetD);
  fprintf(stderr, "Manager TX D packet: %s\n", sb_packet_to_str(packetD, NBYTES).c_str());

}

int main() {
  SBTX client_0_a;
  SBRX client_0_d;

  SBTX manager_0_d;
  SBRX manager_0_a;

  // initialize connections
  fprintf(stderr, "client_0_a initializing\n");
  client_0_a.init("client_0_a.q");
  fprintf(stderr, "client_0_d initializing\n");
  client_0_d.init("client_0_d.q");

  fprintf(stderr, "manager_0_d initializing\n");
  manager_0_d.init("manager_0_d.q");
  fprintf(stderr, "manager_0_a initializing\n");
  manager_0_a.init("manager_0_a.q");

  // form packet
  std::thread tClient0(client_thread, &client_0_a, &client_0_d);

  // receive packet
  std::thread tManager0(manager_thread, &manager_0_a, &manager_0_d);

  tClient0.join();
  tManager0.join();
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
