// Copyright (c) 2024 Zero ASIC Corporation
// This code is licensed under Apache License 2.0 (see LICENSE for details)

#include "switchboard.hpp"
#include "tilelinklib.hpp"

#include <iostream>
#include <thread>
#define NBYTES 32

void client_thread(ClientTLAgent *client) {

  TLMessageA tl_a;
  tl_a.opcode = PutFullData;
  tl_a.param = 0;
  tl_a.size = 4; // 8 bytes
  tl_a.corrupt = 0;
  tl_a.source = 0;
  tl_a.address = 0xDEAD0;
  tl_a.mask = 0xFF;
  for (int i = 0; i < 8; i++) {
    tl_a.data[i] = i;
  }

  // send packet
  client->send_a(tl_a);
  client->print_a(tl_a);

  // TLMessageA *tl_a = (TLMessageA *)packetA.data;
  tl_a.opcode = PutFullData;
  tl_a.param = 0;
  tl_a.size = 4; // 8 bytes
  tl_a.corrupt = 0;
  tl_a.source = 0;
  tl_a.address = 0xDEAD0;
  tl_a.mask = 0xFF;
  for (int i = 0; i < 8; i++) {
    tl_a.data[i] = i;
  }

  // send packet
  client->send_a(tl_a);
  client->print_a(tl_a);

  TLMessageD tl_d;
  client->recv_d(tl_d);
  client->print_d(tl_d);
  assert(tl_d.opcode == AccessAck);
  assert(tl_d.denied == 0);
}

void manager_thread(ManagerTLAgent *manager) {
  TLMessageA tl_a;
  manager->recv_a(tl_a);
  assert(tl_a.opcode == PutFullData);
  assert(tl_a.corrupt == 0);

  manager->recv_a(tl_a);
  assert(tl_a.opcode == PutFullData);
  assert(tl_a.corrupt == 0);

  TLMessageD tl_d;
  tl_d.opcode = AccessAck;
  tl_d.param = 0;
  tl_d.size = 4; // 8 bytes
  tl_d.corrupt = 0;
  tl_d.denied = 0;
  tl_d.source = tl_a.source;
  tl_d.sink = 0;

  // send packet
  manager->send_d(tl_d);
}

int main() {

  ClientTLAgent client("client_0");
  client.set_TLBundleParams("Master", 64, 20, 8, 3);

  ManagerTLAgent manager("manager_0");

  // form packet
  std::thread tClient0(client_thread, &client);

  // receive packet
  std::thread tManager0(manager_thread, &manager);

  tClient0.join();
  tManager0.join();

  printf("PASS!\n");

  return 0;
}
