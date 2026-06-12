# minimal

C++ ↔ RTL exchange via Switchboard queues. `Minimal` DUT increments every byte of each incoming 256-bit packet and sends it back.

Generate RTL before running: `make rtl TARGET=Minimal` (from repo root).

## What happens

1. `send_thread` — sends 32 packets over `in_port.q`; `data[i] = i & 0xff`.
2. `recv_thread` — receives 32 packets from `out_port.q`; asserts `data[i] == (i & 0xff) + 1`.
3. Prints `PASS!`.

```text
TX packet: dest: beefcafe, last: 1, data: {00, 01, 02, ..., 1f}
RX packet: dest: beefcafe, last: 1, data: {01, 02, 03, ..., 20}
...
PASS!
```

## Files

| File | Role |
|------|------|
| `client.cc` | Sends/receives SB packets via `SBTX`/`SBRX` |
| `build.py` | Builds Verilator sim, cleans queues, runs client + DUT |
| `settings.py` | `CHISEL_GEN_RTL_DIR`, `TOP_MODULE` |
| `CMakeLists.txt` | Build targets |

For [build targets](../README.md#cmake-build-targets), [SV wrapper](../README.md#sv-wrapper), and [IDE setup](../README.md#ide-setup-vs-code) — see [sb_sim/README.md](../README.md).

## Dependencies

- [switchboard](https://github.com/zeroasiccorp/switchboard) on `PATH`
- Verilator, C++17 compiler
- RTL under `switchboard.Minimal.None/chisel_gen_rtl/`
