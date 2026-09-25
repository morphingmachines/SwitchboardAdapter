"""Host-side client for the Loopback test fixture.

Usage: loopback_client.py pass|fail|kill
  pass -- send packets, check each comes back unchanged, exit 0
  fail -- exit 1 without touching the queues
  kill -- send the kill packet (dest 0xDEAD), then wait for a reply that never comes
"""

import sys

import numpy as np
from switchboard import PySbPacket, PySbRx, PySbTx

N_PACKETS = 8
N_BYTES = 52  # 416-bit payload


def main(mode: str) -> int:
    if mode == "fail":
        return 1
    tx = PySbTx("in_port.q")
    rx = PySbRx("out_port.q")
    if mode == "kill":
        tx.send(PySbPacket(destination=0xDEAD, flags=1, data=np.zeros(N_BYTES, dtype=np.uint8)))
        rx.recv()
        return 0
    for i in range(N_PACKETS):
        data = np.arange(i, i + N_BYTES, dtype=np.uint8)
        tx.send(PySbPacket(destination=i, flags=1, data=data))
        got = rx.recv()
        if got.destination != i or not np.array_equal(got.data[:N_BYTES], data):
            print(f"mismatch on packet {i}: {got}", file=sys.stderr)
            return 1
    print("PASS!")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
